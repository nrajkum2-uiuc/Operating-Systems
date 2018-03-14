/**
* Password Cracker Lab
* CS 241 - Spring 2018
*/

#include "cracker1.h"
#include "format.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queue.h>
#include <unistd.h>
#include <crypt.h>
typedef struct task {
    char * username;
    char * hash;
    char * password;
    struct task * next;
}task;

typedef struct crack_arg {
    task * task;
    int * index;
}crack_arg;


/* max line length 8: username, 8: password, 13: hash, 2: space, 1: newline*/
const int MAX_LINE_LENGTH = 32;
/* holds thread_count pthreads*/
static pthread_t * pool;
/* queue of tasks */
queue * task_queue;
/* the mutex to make the pthread_status array thread safe*/
static pthread_mutex_t m;
/* the conditional variable you will use */
static pthread_cond_t cv;
/* the number of tasks you're reading in*/
static int tasks = 0;
/* the number of times you have failed to get a password*/
static int fails = 0;
/* the number of times you've succeeded to get a password*/
static int successes = 0;
/* keep track of attempts */
static int attempts = 0;
/* a boolean that shows whether you are reading from stdin or not*/
static int reading = 1;
/* the head of the task_list (meant for freeing the memory)*/
task * task_list_head;

/* this function goes through the linked list and destroys the allocated elements*/
void destroy_task_list(){

  task * curr = task_list_head;
  while(curr){
    // fprintf(stderr, "username: %s, ", (curr->username));
    free((curr->username));
    // fprintf(stderr, "hash: %s, ", (curr->hash));
    free((curr->hash));
    // fprintf(stderr, "password: %s\n", (curr->password));
    free((curr->password));
    task * free_curr = curr;
    curr = curr->next;
    free(free_curr);
  }

}

/* this function reads from stdin and  both:
	- adds to the linked list of tasks
	- pushes to the task_queue
*/
void read_from_stdin(){

  char read_line[MAX_LINE_LENGTH];
  char * read_in = &read_line[0];
  task * curr_task = NULL;
  // fprintf(stderr, "start func read_from_stdin()\n");
  read_in = fgets(read_in,MAX_LINE_LENGTH,stdin);

  while(read_in) {
    // fprintf(stderr, "sizeof task: %zu", sizeof(task));
  //  fprintf(stderr,"Read_in: %s\n", read_in);
    char * strptr;
    char * tok = strtok_r(read_in, " ",&strptr);
	
    task * task_= malloc(sizeof(task));
    if(!task_list_head)
      task_list_head = task_;
    //char * prev = read_in;

    int count = 0;
   		
    while(tok){
      char * str = strdup(tok);
      // fprintf(stderr, "tok: %s\n", tok);
      //str = memcpy(str,(size_t)((char*)tok - (char*)prev));
      if(count == 0)
        task_->username = str;
      else if(count == 1)
        task_->hash = str;
      else if(count == 2){
        task_->password = str;
	int index = getPrefixLength(str);
	while(str[index] == '.')
	    index++;
	task_->password[index] = '\0';
      } else {
	break;
      }
      count++;
      //prev = tok;
      tok = strtok_r(NULL, " ",&strptr);
    }
    if(count != 3) {
	free(task_->username);
	free(task_->password);
	free(task_->hash);
	free(task_);
    } else {
        task_->next = NULL;

        if(curr_task)
          curr_task->next = task_;
        curr_task = task_;
//    fprintf(stderr, "username: %s, pass: %s, hash: %s\n", task_->username, task_->password, task_->hash);
    /* Critical section:
	- pushing to queue and updating task list size
    */
        pthread_mutex_lock(&m);
        queue_push(task_queue, task_);
        tasks++;
        pthread_cond_signal(&cv);
        pthread_mutex_unlock(&m);
    /* Critical section: end */
    }

    read_in = fgets(read_in,MAX_LINE_LENGTH,stdin);
  }
  /* Critical section:
	- set reading to 0, so no thread has to be passive and slee 
  */
  pthread_mutex_lock(&m);
  reading = 0;
  pthread_mutex_unlock(&m);
  /* Critical section: end */
}


void * crack(void * arg){

   
    /*TODO: loop indefinitely until you are:
	- finished reading from stdin AND
	- have no more tasks left to pull from the queue
    */
    void * ptr = arg; 
    while(1) {
        /* Critical section:
		- sleep until you are told to wake up either by a :
			- signal from when soething is pushed into the queue
			- signal from a broadcast at the end, when everything is finished 
	*/
        pthread_mutex_lock(&m);
        while(attempts == tasks && reading) {
	    pthread_cond_wait(&cv,&m);        
        }
	task * task_ = NULL;
        if(attempts <  tasks) {
            task_ = queue_pull(task_queue);
            attempts++;
	    v1_print_thread_start((int)((ptr - (void*)pool)/sizeof(void*)) + 1, task_->username);
	} else {
	    pthread_mutex_unlock(&m);
	    break;
	}
        pthread_mutex_unlock(&m);
        /* Critical section: end */
	double start = getThreadCPUTime();
	double end;
        /* Calculation goes here */
        struct crypt_data cdata;
        cdata.initialized = 0;
        int hashes = 0;
	int result = 0;
        char * password = task_->password;
        int chars_to_copy = getPrefixLength(password);
        size_t password_length = strlen(password);
        char decrypt_password[password_length + 1];
	decrypt_password[password_length] = '\0';
	memcpy(&decrypt_password[0], password, chars_to_copy);
        char * unknown_password = &decrypt_password[chars_to_copy];
	memset(unknown_password,'a',password_length -chars_to_copy);
        char * hash = task_->hash;
	hashes++;
	char * decrypt_hash = crypt_r(decrypt_password, "xx", &cdata);
        while(strcmp(hash,decrypt_hash) != 0) {
	    if(!incrementString(unknown_password)){
		break;
	    }
            decrypt_hash = crypt_r(decrypt_password, "xx", &cdata);
	    hashes++;
	    if(strcmp(hash,decrypt_hash) == 0) {
		result = 1;
	        break;
	    }
        }
	/* Calculation End */
        /* Critical section:
		- update success
		- update failure
	*/
	pthread_mutex_lock(&m);
	end = getThreadCPUTime();
	double var = end - start;
        v1_print_thread_result((int)((ptr - (void*)pool)/sizeof(void*)) + 1, task_->username, decrypt_password, hashes,var, !result);
	if(result)
	    successes++;
	else
	    fails++;
	pthread_mutex_unlock(&m);
	/* Critical section: end */
    }
    return NULL;
}

/*TODO: init the thread pool by:
	- allocating space for thread_count pthread_ts 
	- creating the pthreads to the crack start routine with argument "i" <- thread number
*/
void init_thread_pool(size_t thread_count) {
    
    pool = malloc(sizeof(pthread_t) * thread_count);
    
    for(size_t i = 0; i < thread_count; i++) {
	pthread_create(&pool[i],NULL, crack, &pool[i]);
    }

}

/*TODO: clean up the threads by:
	- joining all the threads that are left by waiting on them.
	- freeing the allocated thread pool
*/
void cleanup_threads(size_t thread_count) {
    pthread_cond_broadcast(&cv);
    for(size_t i = 0; i < thread_count; i++){
	pthread_join(pool[i], NULL);
    }
    free(pool);
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&cv);
}

/*TODO: main routine here */

int start(size_t thread_count) {

    // TODO your code here, make sure to use thread_count!
    // Remember to ONLY crack passwords in other threads
    task_list_head = NULL;
    task_queue = queue_create(-1);
    /* initialize your mutex and conditional vars */
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&cv, NULL);
    init_thread_pool(thread_count);
    /*TODO: Read in from STDIN (where the file has been redirected)
            - Each "task" is delimited by a "newline" (you will have to malloc)
            - The end of the tasks are marked by an end of file
            - [username]SPACE[hash]SPACE[password]
    */
    read_from_stdin();
    


    /*TODO: cleanup
      - destroy the task_list
      - destroy the queue
      - join your pthreads
      - destroy your mutex
      - destroy your status array
    */
    cleanup_threads(thread_count);
    queue_destroy(task_queue);
    destroy_task_list();
    v1_print_summary(successes,fails);
    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally

}
