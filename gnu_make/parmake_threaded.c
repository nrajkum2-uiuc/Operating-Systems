/**
* Parallel Make Lab
* CS 241 - Spring 2018
*/
#include <stdio.h>
#include <stdlib.h>
#include "rule.h"
#include "format.h"
#include "graph.h"
#include "parmake.h"
#include "parser.h"
#include "set.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "queue.h"
#include <pthread.h>
static graph * dependencies = NULL;
static vector ** goal_rules = NULL;
static size_t num_goals = 0;

/* Preserving mutual exclusion and preventing race conditions */
static pthread_mutex_t m;
static pthread_cond_t cv;
static pthread_cond_t no_busy_wait;
static pthread_mutex_t no_busy_mutex;

static int no_busy = 1;
static int rule_available = 0;
static int attempts = 0;
static int to_attempt = 0;

/* pthread array to be set to num_threads */
static pthread_t * pthreads;
static queue * rules;

void cleanup_goal_rules() {
    for(size_t i = 0; i < num_goals; i++) {
	if(goal_rules[i])
	    vector_destroy(goal_rules[i]);
    }
}

// O(N) lookup. I really would like constant time lookup though.
int vector_contains(vector * vec, void * val) {
    size_t i = 0;
    for(; i < vector_size(vec); i++) {
	if(val == vector_get(vec,i) )
	    return 1;
    }
    return 0;
}

void print_goals() {
    size_t i = 0;
    for(; i < num_goals; i++) {
	vector *it = goal_rules[i];
	if(it == NULL) {
	    fprintf(stderr, "not executed\n");
	} else {
	    size_t j = 0;
	    for(; j < vector_size(it); j++) {
//	        rule_t * rule = graph_get_vertex_value(dependencies,vector_get(it,i));
//		vector * commands = rule->commands;
//		for(size_t k = 0; k < vector_size(commands); k++) {
//		    fprintf(stderr, "command: %s\n", (char*)vector_get(commands,k));
////		}
//		fprintf(stderr, "rule->target: %s\n", rule->target);
		fprintf(stderr, "= %s =", (char*)vector_get(it,j));
		
	     }
	    fprintf(stderr, "\n");
	}
    }
}


int dfs(void * vertex,set * black_set,set * gray, vector * black) {
//   fprintf(stderr, "dfs at: %s\n", (char*)vertex);
    vector * neighbors = graph_neighbors(dependencies,vertex);
    size_t i = 0;
    int cycle_detected = 0;
    set_add(gray,vertex);
    for(; i < vector_size(neighbors); i++) {
	if(!set_contains(gray,vector_get(neighbors,i)))
	    cycle_detected += dfs(vector_get(neighbors,i),black_set, gray, black);
	else if(!set_contains(black_set,vector_get(neighbors,i))){
	    cycle_detected = 1;
//	    fprintf(stderr, "contains: %s\n", (char*)vector_get(neighbors,i));
        }
    }
    vector_destroy(neighbors);
    //void * insert_vertex = malloc(sizeof(void*));
    //insert_vertex = vertex;
    set_add(black_set,vertex);
    vector_push_back(black,vertex);
    return cycle_detected;
}

void dfs_valid(vector * goals) {
    /*TODO: take each goal rule from the front off the goals vector
	- pop it off
	- dfs until all descendants are searched through 
		- you will need to mark as grey/black by putting it in two separate vectors (g/b)
		- doing this will help with:wq
 cycle detection (if a gray is detected)
		- if a grey is detected: remove from dependency graph
		- if you complete the dfs without cycles then you should add to valid_rules
    */
        //rule_t * goal_rule = graph_get_vertex_value(dependencies,goal_vertex);
	size_t i = 0;
	for(; i < num_goals; i++) {
	    set * gray = shallow_set_create();
	    vector * black = shallow_vector_create();
	    set * black_set = shallow_set_create();
	    void * goal = vector_get(goals,i);
	    int cycle = dfs(goal,black_set,gray,black);
//	    fprintf(stderr, "done\n");
	    if(!cycle) {
		goal_rules[i] = black;
	    } else {
		rule_t * rule = graph_get_vertex_value(dependencies,vector_get(goals,i));
		print_cycle_failure(rule->target);
		goal_rules[i] = NULL;
		vector_destroy(black);
	    }
	    set_destroy(black_set);
	    set_destroy(gray);
	}
	
}

void * execute_commands(void * arg) {

// ---------------- main loop that will only exit upon receiving a NULL in the queue ----- //
 
    while(1) {

	/* Critical section: start*/     
	pthread_mutex_lock(&m);
        while(!rule_available) {
	    pthread_cond_wait(&cv,&m);
        }
	void * rule_vertex = queue_pull(rules);
	attempts++;
	rule_available = to_attempt > attempts;
	if(rule_vertex == NULL) {
	    pthread_mutex_unlock(&m);
	    
	    pthread_mutex_lock(&no_busy_mutex);
            no_busy = 0;
            pthread_cond_signal(&no_busy_wait);
            pthread_mutex_unlock(&no_busy_mutex); 
	    
	    return NULL;
	}
	rule_t * rule = (rule_t*)graph_get_vertex_value(dependencies,rule_vertex);
	vector * commands = rule->commands;
	pthread_mutex_unlock(&m);
        /* Critical section: end*/

        int to_execute = 1;

// ---------- this section of code handles the case where the rule is a file name ---- //
        int found = access(rule->target,F_OK);
            // determine whether dependencies of rule is file on disk

            // find neighbors of current rule

	/* Critical section: start */
	pthread_mutex_lock(&m);
        vector * dependants = graph_neighbors(dependencies,(void*)rule_vertex);
	pthread_mutex_unlock(&m);
	/* Critical section: end */
	
        vector * found_on_disk = shallow_vector_create();

            // check whether that this point in the flow depends on a file on the system
        for(size_t l = 0; l < vector_size(dependants); l++) {
	    /* Critical section: start */
	    pthread_mutex_lock(&m);
            rule_t * neighbor_rule = graph_get_vertex_value(dependencies,vector_get(dependants,l));
	    pthread_mutex_unlock(&m);
	    /* Critical section: end */
            if(!access(neighbor_rule->target,F_OK))
                vector_push_back(found_on_disk,neighbor_rule);
        }

        int zero_dependants = 0;
        if(!vector_size(dependants))
            zero_dependants = 1;

        if(!found) {
            if(vector_size(found_on_disk)) {
                to_execute = 0;
                struct stat statbuf;
                struct stat statbuf_;
                stat(rule->target,&statbuf_);
                time_t t_rule = statbuf_.st_mtime;
                for(size_t l = 0; l < vector_size(found_on_disk); l++) {
                    stat(((rule_t*)(vector_get(found_on_disk,l)))->target,&statbuf);
                    time_t t_dep = statbuf.st_mtime;
                    if(difftime(t_rule,t_dep) < 0) {
                        to_execute = 1;
                    }
                }
            }
            if(zero_dependants)
                to_execute = 0;
        }
        vector_destroy(dependants);
        vector_destroy(found_on_disk);
// ---------------- end of file name check ---------------------- //

// ---------------  time to execute commands ------------------- //
        size_t k = 0;

        if(!to_execute) {
	    /* Critical section: start */
	    pthread_mutex_lock(&m);
	    rule->state = 1;
	    pthread_mutex_unlock(&m);
	    /* Critical section: end */
	    pthread_mutex_lock(&no_busy_mutex);
	    no_busy = 0;
	    pthread_cond_signal(&no_busy_wait);
	    pthread_mutex_unlock(&no_busy_mutex);
            continue;
	}
	int failed = 0;
        for(; k < vector_size(commands); k++) {
//	    fprintf(stderr, "rule->target: %s\n", rule->target);
            failed = system(vector_get(commands,k));
            if(failed) {
		/* Critical section: start */
		pthread_mutex_lock(&m);
//		fprintf(stderr, "failed\n");
                rule->state = -1;
		pthread_mutex_unlock(&m);
		/* Critical section: end */
//                fprintf(stderr, "failed: %s\n", (char*)vector_get(commands,k));
                break;
            } 
	}
	if(!failed) {
	     /* Critical section: start */
             pthread_mutex_lock(&m);
             rule->state = 1;
             pthread_mutex_unlock(&m);
	     /* Critical section: end */
	}
	pthread_mutex_lock(&no_busy_mutex);
	no_busy = 0;
	pthread_cond_signal(&no_busy_wait);
	pthread_mutex_unlock(&no_busy_mutex);
		
// -------------- finished executing commands ---------------//

    }
// ------------ end of main loop body -------------//
    return NULL;
}

int parmake(char *makefile, size_t num_threads, char **targets) {

// ------------------- multithreading setup : includes init of mutexes, queue and start threads -- //    // initializing mutexes, pthreads
    pthread_mutex_init(&m,NULL);
    pthread_cond_init(&cv,NULL);
    pthread_cond_init(&no_busy_wait,NULL);
    pthreads = (pthread_t*) (malloc(sizeof(pthread_t) * num_threads)); 
    // initiate queue
    rules = queue_create(-1);
    // start the pthreads 
    for(size_t i = 0; i < num_threads ; i++) {
	pthread_create(&pthreads[i], NULL, &execute_commands, NULL);
    }    
// -------------------- setup finished ------------------- //


// ---------------------- detect cycles and decide which goal rules are viable ---------- //
    // TODO: Parse graph and start up threads 
    dependencies = parser_parse_makefile(makefile,targets);
    // TODO: get the goal rules from the graph by finding the neighbors of sentinel ''
    void * sentinel = (void*)("");
    vector * goals = graph_neighbors(dependencies, sentinel);
    num_goals = vector_size(goals);
    goal_rules = (vector**)malloc(sizeof(vector*)*num_goals);
    /* TODO: DFS through the goal rules and decide whether they need to be removed on the four cond.
	- the rule is a goal rule, but is involved in a cycle, i.e. there exists a path from the rule to itself in the dependency graph
	- the rule is a goal rule, but at least one of its descendants, i.e. any rule in the dependency graph reachable from the goal rule, is involved in a cycle
	- the rule is not a goal rule, and it has no ancestors that are goal rules
	- the rule is not a goal rule, and all of its goal rule ancestors fall under (1) or (2)	
    */
    dfs_valid(goals);
    // destroy goals vector
    vector_destroy(goals);
//    print_goals();
// ------------------ finished detecting cycles ----------------------- //

//    print_goals();
// ------------------- this section below pushes rules onto the rules queue ------- //
    /* iter through the goal_rules */
    size_t i =0;
    while(i < num_goals) {

	vector * goal = goal_rules[i];
	size_t j = 0;
	if(!goal){
	    i++;
	    continue;
        }
	
	size_t goal_size = vector_size(goal);
	size_t rule_count = 0;
	int rule_resolved[goal_size];
	// init rule_pushed to all 0
	memset(rule_resolved, 0,sizeof(int)*goal_size);
	while(rule_count < goal_size){
	while(j < goal_size) {
	    /* Check if a rule is available */
	    // check the neighbors of the the j'th item in the vector 
	    // if any of the neighbrs are not ->state == 1, then a rule is not available yet
	    // in the case where any of the neighbors' ->state == -1, then the rule is failed
	    // mark it as failed and increment j 
	    // j should be incremented in every case except in the case where all neighbrs have state == 0 
	    if(rule_resolved[j]){
		j++;
		continue;
	    }
	    /* Critical section: start */
	    pthread_mutex_lock(&m);
	    void * rule_vertex = vector_get(goal,j);
	    rule_t * rule = (rule_t*)graph_get_vertex_value(dependencies,rule_vertex);
            vector * neighbors = graph_neighbors(dependencies, rule_vertex);
	    int check_neighbors = 1;
	        for(size_t k = 0; k < vector_size(neighbors); k++) {
		    rule_t * neighbor_rule = (rule_t*)graph_get_vertex_value(dependencies,vector_get(neighbors,k));
	//	    fprintf(stderr, "neighbor->target: %s ", neighbor_rule->target);
	//	    fprintf(stderr, "neighborstate: %d\n", neighbor_rule->state);
		    if(neighbor_rule->state == -1){
//			fprintf(stderr, "failed over here\n");
		        rule->state = -1;
		    }
		    if(neighbor_rule->state < 1){
		        check_neighbors = 0;
		        if(rule->state == -1)
			    break;
		    }
	        }
//	    fprintf(stderr, "rule->target && check_neighbirs: %s && %d\n", rule->target,check_neighbors);
	    if(check_neighbors || rule->state == -1){
		if(check_neighbors) {
	        queue_push(rules,rule_vertex);
		to_attempt++;
		rule_available = to_attempt > attempts;
		}
		rule_count++;
		if(rule->state ==-1)
		    rule_resolved[j] = 2;
		else
		    rule_resolved[j] = 1;
	    } 
//            fprintf(stderr, "to_attempt: %d, attempt: %d\n", to_attempt, attempts);
	    pthread_cond_signal(&cv);
	    pthread_mutex_unlock(&m);
	    /* Critical section: end */
	    j++;	
	    vector_destroy(neighbors);
	    // if the rule is NOT  waiting on any dependencies and they all have not failed 
	}
//	fprintf(stderr, "dedlok: %zu, %zu\n", rule_count, goal_size);
	if(rule_count == goal_size) {
	    pthread_mutex_lock(&no_busy_mutex);
	    no_busy = 0;
	    pthread_mutex_unlock(&no_busy_mutex);
	}
	pthread_mutex_lock(&no_busy_mutex);
	while(no_busy){
	    pthread_cond_wait(&no_busy_wait,&no_busy_mutex);
	}
	no_busy = 1;
	pthread_mutex_unlock(&no_busy_mutex);
//	fprintf(stderr, "fucked\n");
	j = 0;
//	fprintf(stderr, "over here!!\n");	
	}
        i++;
    }
// ----------------- this section above pushes rules onto the rules queue ------ // 


// ---------- wake up dormant threads and let them join ---------- // 
//    fprintf(stderr, "here\n");
    // push num_threads NULLs onto the queue so that the start routine can break from main loop
    /* Critical section: start */
    pthread_mutex_lock(&m);
    for(i = 0; i < num_threads; i++) {
	to_attempt++;
	queue_push(rules,NULL);
	rule_available = to_attempt > attempts;
    }
    pthread_mutex_unlock(&m);
    /* Critial sectiom: end */
  //  fprintf(stderr, "over here\n");
    // clean up threads by broadcasting and joining on all of them 
    pthread_cond_broadcast(&cv);
    for(i = 0; i < num_threads; i++) {
	pthread_join(pthreads[i],NULL);
    }
// ---------- threads should all have joined by this point ----------- //

// --------- section frees up  memory ------------ // 
    pthread_mutex_destroy(&no_busy_mutex);
    pthread_cond_destroy(&no_busy_wait);
    // destroy dependencies graph
    graph_destroy(dependencies);
    // destroy the queue
    queue_destroy(rules);
    // destroy mutexes
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&cv);
    // free pthreads
    free(pthreads);
    cleanup_goal_rules();
    free(goal_rules);
// ------- section finished with freeing memory ---------- //
    return 0;
}
