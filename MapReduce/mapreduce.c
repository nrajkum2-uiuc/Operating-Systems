/**
*  Lab
* CS 241 - Spring 2018
*/

#include "utils.h"
#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define FD_STDIN 0
#define FD_STDOUT 1
static char * output_filename = NULL;
static char * input_filename = NULL;
void setup_fds(int new_stdin, int new_stdout);
void close_and_exec(char *exe, char *const *params);
pid_t start_reducer(char *reducer_exec, int in_fd, char *output_filename);
pid_t *read_input_chunked(char *filename, int *fds_to_write_to,
                          int num_mappers);
pid_t *start_mappers(char *mapper_exec, int num_mappers, int *read_mapper,
                     int write_reducer);
size_t count_lines(const char *filename);

void usage() {
    print_usage();
}

int main(int argc, char **argv) {
// ----------------- check that program has been called properly ------- //
    if(argc != 6){
	usage();
	exit(1);
    }
    if(access(argv[1], F_OK) == -1 || access(argv[3], F_OK) == -1 || access(argv[4], F_OK)  == -1 ){
	usage();
	exit(1);
    }
//    if(access(argv[1], F_OK) == -1 || access(argv[3], F_OK) == -1 || access(argv[4], F_OK) == -1 )  {
//	usage();
//	exit(1);
//    }
    output_filename = argv[2];
    input_filename = argv[1];
    int num_maps = atoi(argv[5]);
    if(num_maps <= 0) {
	usage();
	exit(1);
    }
// --------------------------------------  end of check -------------- //
    
// ------------------------ create an input pipe for each mapper ------ //

    // every mapper will be within a child process
    // you will need lotsa pipes since every process has to communicate with each other (num_maps) mappers + 1 reducer + (num_maps) splitters 
     /* 
	[n] splitter0 ------ mapper 0 ------------------==reducer 
	    splitter1 ------ mapper 1 -----------------/
	    .................mapper 2 ----------------/
	    .................mapper 3 ---------------/
    */ 
    // all mappers feed into one reducer, so you should dup the reducer[READ] into all mapper[WRITE]

    int total_processes = num_maps*2 + 1;
    int pipefd[num_maps+1][2];
    pid_t  processes[total_processes];
    // will hold information about failed proccesses
    int status_arr[total_processes];
    memset(status_arr,0,sizeof(int)*total_processes); 

// ------------------------------ fibished setting up pipes ------------------ //
    for(int i = 0; i < num_maps+1; i++) {
	pipe(pipefd[i]);
    }

    int output_fd = open(output_filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
     
    for(int i = 0; i < num_maps; i++) {
	
	processes[i] = fork();
	if(processes[i] == 0) {
	    for(int j = i; j < num_maps; j++) {
		if( i != j) { close(pipefd[j][0]); close(pipefd[j][1]);}
	    }
	    close(output_fd);
	    close(pipefd[i][0]);
	    close(pipefd[num_maps][0]);
	    close(pipefd[num_maps][1]);
	    dup2(pipefd[i][1],STDOUT_FILENO);
	    char * num = NULL;
	    asprintf(&num, "%d",i);
	    status_arr[i] = execlp("./splitter","./splitter",input_filename,argv[5],num,(char*)NULL);    
	    exit(1);
	} else {
	    close(pipefd[i][1]);
	}
    }
    
    for(int i = 0; i < num_maps; i++) {

	processes[i+ num_maps] = fork();
	
	if(processes[i + num_maps] == 0) {
	    for(int j = i; j < num_maps; j++) {
		if( i != j ) { close(pipefd[j][0]); close(pipefd[j][1]); }
	    }
	    close(output_fd);
	    close(pipefd[i][1]);
	    close(pipefd[num_maps][0]);
	    dup2(pipefd[i][0],STDIN_FILENO);
	    dup2(pipefd[num_maps][1],STDOUT_FILENO);
	    status_arr[i+num_maps] = execlp(argv[3], argv[3], NULL);
	    exit(1);
        } else {
	    close(pipefd[i][0]);
	}
    }

    for(int i = 0; i < num_maps; i++) {
	close(pipefd[i][0]);
	close(pipefd[i][1]);
    }
    close(pipefd[num_maps][1]);

    processes[num_maps*2] = fork();
    
    if(processes[num_maps*2] == 0) {
	dup2(pipefd[num_maps][0],STDIN_FILENO);
	dup2(output_fd,STDOUT_FILENO);
	execlp(argv[4],argv[4],NULL);
	exit(1);
    }
    close(pipefd[num_maps][0]);
    close(pipefd[num_maps][1]);
    close(output_fd);
    for(int i = 0; i < total_processes; i++) {
	//fprintf(stderr, "statusarr[i]: %d\n", status_arr[i]);
	waitpid(processes[i],&status_arr[i],0);
	//fprintf(stderr, "statusarr[i]: %d\n", status_arr[i]);
	if(!WIFEXITED(status_arr[i])) {
	    if(i < num_maps) {
		print_nonzero_exit_status("./splitter", status_arr[i]);
	    } else if(i >= num_maps && i < total_processes-1) {
		print_nonzero_exit_status(argv[3], status_arr[i]);
            } else {
		print_nonzero_exit_status(argv[4], status_arr[i]);
	    }
	}
    }
    print_num_lines(output_filename);
    // ---------- wait on all processes ( splitters, mappers, and then reducer ---//

    // Create an input pipe for each mapper.
    


    // Create one input pipe for the reducer.

    // Open the output file.

    // Start a splitter process for each mapper.

    // Start all the mapper processes.

    // Start the reducer process.

    // Wait for the reducer to finish.

    // Print nonzero subprocess exit codes.

    // Count the number of lines in the output file.

    return 0;
}
