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
static graph * dependencies = NULL;
static vector ** goal_rules = NULL;
static size_t num_goals = 0;

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

void execute_commands() {
    size_t i = 0;
    for(; i < num_goals; i++) {
	if(!goal_rules[i])
	    continue;
//	fprintf(stderr, "%p\n", goal_rules[i]);
        size_t j = 0;
	vector * flow = goal_rules[i];
	vector * failed_set = shallow_vector_create();


	// traverse through the rule flow to try to satisfy the rule
	for(; j < vector_size(flow); j++) {
	    int to_execute = 1;


	    // check if this current point in the flow depends on any failed part of the flow eariler
	    for(size_t l = 0; l < vector_size(failed_set); l++) {
                if(graph_adjacent(dependencies,vector_get(flow,j),vector_get(failed_set,l))) {
		    ((rule_t*)(graph_get_vertex_value(dependencies,vector_get(flow,j))))->state = -1;
		    vector_push_back(failed_set,vector_get(flow,j));
		    to_execute = 0;
		    break;
		}
	    }

	    if(!to_execute)
		continue;
	    rule_t * rule = graph_get_vertex_value(dependencies,vector_get(flow,j));
	    vector * commands = rule->commands;
	    // determine whether rule name is a file on disk
            int found = access(rule->target,F_OK);
            // determine whether dependencies of rule is file on disk
            // find neighbors of current rule
            vector * dependants = graph_neighbors(dependencies,vector_get(flow,j));
            vector * found_on_disk = shallow_vector_create();


	    // check whether that this point in the flow depends on a file on the system
            for(size_t l = 0; l < vector_size(dependants); l++) {
                rule_t * neighbor_rule = graph_get_vertex_value(dependencies,vector_get(dependants,l));
                if(!access(neighbor_rule->target,F_OK)) 
                    vector_push_back(found_on_disk,neighbor_rule);
            }
	    int zero_dependants = 0;
	    if(!vector_size(dependants))
		zero_dependants = 1;
	    vector_destroy(dependants);
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
	    vector_destroy(found_on_disk);
	    // execute commands
	    size_t k = 0;
	    rule->state = 1;
	    if(!to_execute)
		continue;
	    for(; k < vector_size(commands); k++) {
		int failed = 0;
		failed = system(vector_get(commands,k));
		if(failed) {
		    rule->state = -1;
//		    fprintf(stderr, "failed: %s\n", (char*)vector_get(commands,k));
		    vector_push_back(failed_set,vector_get(flow,j));
		    break;
		} 
    	    }
	}
	vector_destroy(failed_set);
    }

}

int parmake(char *makefile, size_t num_threads, char **targets) {
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
//    print_goals();
    execute_commands();
    // destroy dependencies vector
    graph_destroy(dependencies);
    // remember that every vector returned from a graph method needs to be destroyed
    vector_destroy(goals);
    cleanup_goal_rules();
    free(goal_rules);
    return 0;
}
