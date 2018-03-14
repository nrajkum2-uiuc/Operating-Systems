/**
* Password Cracker Lab
* CS 241 - Spring 2018
*/
#include <unistd.h>
#include "cracker2.h"
#include "format.h"
#include "utils.h"
#include <stdlib.h>
#include <crypt.h>
#include <string.h>
#include <stdio.h>
/* Globals go here */

static int attempts = 0;

static int tasks = 0;

static double begin = 0;

static double end = 0;

static double program_begin = 0;

static double program_curr = 0;

static int end_reading = 0;

static pthread_cond_t cv;

static pthread_mutex_t m;

static pthread_barrier_t b;

static int total_hashes = 0;

static pthread_t * workers;

typedef struct task {
    char * username;
    char * password;
    char * hash;
    long  count;
    long  start;
} task;

task * task_array = NULL;

static int result = 1;

const int MAX_LINE_LENGTH = 32;

static char * found_password = NULL;

void init_globals(size_t threads) {

    task_array = malloc(sizeof(task) * threads);
    pthread_cond_init(&cv, NULL);
    pthread_mutex_init(&m, NULL);
    pthread_barrier_init(&b, NULL, threads + 1);
    workers = malloc(sizeof(pthread_t) * threads);

}

void destroy_everything(size_t threads) {

    pthread_cond_broadcast(&cv);
    for(size_t i = 0; i < threads; i++) {
	pthread_join(workers[i],NULL);
    }
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&m);
    pthread_barrier_destroy(&b);
    free(workers);
    free(task_array);

}

void * crack(void * thread_id) {
    int threadId = (int)( ( thread_id - (void*)workers)/sizeof(void*) + 1);

    while(1) {
        
        /* Critical section start */
        pthread_mutex_lock(&m);
        while(attempts == tasks && !end_reading) {
	    pthread_cond_wait(&cv, &m);
	   // fprintf(stderr,"wait!\n");
        }
	//fprintf(stderr, "out of the sleep\n");
        if( end_reading ) {
	    pthread_mutex_unlock(&m);
            break;
        }
	attempts++;
        task * task_ = &task_array[threadId-1];
        pthread_mutex_unlock(&m);
        /* Critical section end */
        
	v2_print_thread_start(threadId, task_->username, (task_->start) , task_->password);
        /* Calculation start */
        struct crypt_data cdata;
        cdata.initialized = 0;
        int hashes = 0;
        char * password = task_->password;
        char * hash = task_->hash;
        char * decrypt_hash = NULL;
	int result_code = -1;
	long count = (task_->count);
	long i = 0;
	for(; i < count; i++) {
            /* Critical section start */
	    pthread_mutex_lock(&m);
	    if( result == 0 ) {
	        pthread_mutex_unlock(&m);
	        result_code = 1;
		break;
            }
 	    pthread_mutex_unlock(&m);
	    /* Critical section end */	
            
            decrypt_hash = crypt_r(password, "xx", &cdata);
            hashes++;
            if(strcmp(hash,decrypt_hash) == 0) {
		/* Critical section start */
		pthread_mutex_lock(&m);
		result = 0;
		found_password = password;
	        pthread_mutex_unlock(&m);
		/* Critical section end */
	        result_code = 0;
                break;
            }
	    incrementString(password);
        }
	pthread_mutex_lock(&m);
	total_hashes+=hashes;
	pthread_mutex_unlock(&m);
	if( i == count && result_code == -1  )
	     result_code = 2;
        /* Calculation End */
        v2_print_thread_result(threadId, hashes, result_code);
	pthread_barrier_wait(&b);
    }
    return NULL;
}

void init_workers(size_t threads) {
    
    for(size_t i = 0; i < threads; i++) {
	pthread_create(&workers[i], NULL, crack, &workers[i]);
    }

}

int start(size_t thread_count) {
    init_globals(thread_count);
    init_workers(thread_count);
    // TODO your code here, make sure to use thread_count!
    // Remember to ONLY crack passwords in other threads
    // DO NOT change the return code since AG uses it to check if your
              // program exited normally

    char read_line[MAX_LINE_LENGTH];
    char * read_in = &read_line[0];
    task * base_task_ = NULL;
    read_in = fgets(read_in,MAX_LINE_LENGTH,stdin);
    while(read_in) {
	char * strptr;
        //fprintf(stderr, "read_in: %s\n", read_in);
	char * tok = strtok_r(read_in, " ",&strptr);
	//fprintf(stderr, "is malloc working\n");
        base_task_ = malloc(sizeof(task));
        //fprintf(stderr, "malloc is working\n");
        int count = 0;
        //fprintf(stderr, "am i tokenizing properly\n"); 
        //fprintf(stderr, "base_task_: %p\n", base_task_);
	while(tok) {
	    //fprintf(stderr, "tok: %s && count:%d\n", tok, count);
	    if ( count == 0 )
		base_task_->username = (tok);
	    else if ( count == 1 )
		base_task_->hash = (tok);
	    else if ( count == 2 ) {
		base_task_->password = (tok);
	        int index = getPrefixLength(tok);
		while(tok[index] == '.')
		    index++;
		base_task_->password[index] = '\0';
	    } else { break; }
            tok = strtok_r(NULL, " ",&strptr);
	    count++;
	    //fprintf(stderr, "task_username: %s\n", base_task_->username);
        }
	//fprintf(stderr, "yeah i am tokenizing properly, and count is: %d\n", count);
        if( count == 3 ){
	     
	    int unknown_letter_count = strlen(base_task_->password) - getPrefixLength(base_task_->password);
	    long attempt_num[1];
	    long start_index[1];
	    //fprintf(stderr, "heap @: %p\n", sbrk(0));
	    for(size_t i = 0; i < thread_count; i++) {
		//fprintf(stderr,"seg fault\n");
		//fprintf(stderr, "a seg fault\n");
	        getSubrange(unknown_letter_count,thread_count,i+1,&start_index[0],&attempt_num[0]);
		//fprintf(stderr, "no seg fault yet\n");
		//fprintf(stderr, "task_array[i]: %p, username: %s\n", &task_array[i], task_array[i].username);
	        //fprintf(stderr, "base_task_->username: %s\n", base_task_->username);
		/* Critical section start */
		pthread_mutex_lock(&m);
	        task_array[i].username = strdup(base_task_->username);
		//fprintf(stderr, "what?\n");                                   
                task_array[i].hash = strdup(base_task_->hash);
	        char * password = strdup(base_task_->password);
	        int dots = getPrefixLength(base_task_->password);
	        memset(&password[getPrefixLength(password)], 'a', unknown_letter_count);
	        setStringPosition(&password[dots], start_index[0]);
	        task_array[i].password = password;
		task_array[i].count = attempt_num[0];
		task_array[i].start = start_index[0];
		pthread_mutex_unlock(&m);
	        /* Critical section end */
//                fprintf(stderr, "username: %s, paswword: %s, start_index: %ld \n", task_array[i].username, password, start_index);
	    }
	    begin = getTime();
	    program_begin = getCPUTime();
	    /* Critical section start */
	    pthread_mutex_lock(&m);
	    //fprintf(stderr, "broadcasting\n");
	    tasks+=thread_count;
	    //fprintf(stderr, "attempts: %d, tasks: %d\n", attempts, tasks);
	    pthread_cond_broadcast(&cv);
	    pthread_mutex_unlock(&m);
	    /* Critical section end */
	    
	    pthread_barrier_wait(&b);
	    end = getTime();
	    program_curr = getCPUTime();
        
            double diff = end - begin;
	    double program_time = program_curr - program_begin;

            /* Critical section start */
	    pthread_mutex_lock(&m);
            v2_print_summary(base_task_->username, found_password, total_hashes, diff, program_time, result);
	    result = 1;
	    total_hashes = 0;
	    found_password = NULL;
            for(size_t i = 0; i < thread_count; i++) {
		free(task_array[i].username);
		free(task_array[i].password);
		free(task_array[i].hash);
		task_array[i].username = NULL;
		task_array[i].password = NULL;
		task_array[i].hash = NULL;
	    }
	    pthread_mutex_unlock(&m);
	    /* Critical section end */

        }
	//fprintf(stderr, "do i seg fault\n");
	free(base_task_);
	base_task_ = NULL;
	//fprintf(stderr, "i don't!\n");
        read_in = fgets(read_in, MAX_LINE_LENGTH, stdin);

    }
    /* Critical section start */
    pthread_mutex_lock(&m);
    end_reading = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&m);
    /* Critical section end */
    destroy_everything(thread_count);        
    return 0;
}
