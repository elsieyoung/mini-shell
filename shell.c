#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcheck.h>

#include "parser.h"
#include "shell.h"

/**
 * Program that simulates a simple shell.
 * The shell covers basic commands, including builtin commands 
 * (cd and exit only), standard I/O redirection and piping (|). 
 */

#define MAX_DIRNAME 100
#define MAX_COMMAND 1024
#define MAX_TOKEN 128

/* Functions to implement, see below after main */
int execute_cd(char** words);
int execute_nonbuiltin(simple_command *s);
int execute_simple_command(simple_command *cmd);
int execute_complex_command(command *cmd);


int main(int argc, char** argv) {
	char cwd[MAX_DIRNAME];           /* Current working directory */
	char command_line[MAX_COMMAND];  /* The command */
	char *tokens[MAX_TOKEN];         /* Command tokens (program name, 
					  				  * parameters, pipe, etc.) */

	while (1) {

		/* Display prompt */		
		getcwd(cwd, MAX_DIRNAME-1);
		printf("%s> ", cwd);
		
		/* Read the command line */
		fgets(command_line, MAX_COMMAND, stdin);
		/* Strip the new line character */
		if (command_line[strlen(command_line) - 1] == '\n') {
			command_line[strlen(command_line) - 1] = '\0';
		}
		
		/* Parse the command into tokens */
		parse_line(command_line, tokens);

		/* Check for empty command */
		if (!(*tokens)) {
			continue;
		}
		
		/* Construct chain of commands, if multiple commands */
		command *cmd = construct_command(tokens);
		//print_command(cmd, 0);
	
		int exitcode = 0;
		if (cmd->scmd) {
			exitcode = execute_simple_command(cmd->scmd);
			if (exitcode == -1) {
				break;
			}
		}
		else {
			exitcode = execute_complex_command(cmd);
			if (exitcode == -1) {
				break;
			}
		}
		release_command(cmd);
	}
    
	return 0;
}


/**
 * Changes directory to a path specified in the words argument;
 * For example: words[0] = "cd"
 *              words[1] = "csc209/assignment3/"
 * Your command should handle both relative paths to the current 
 * working directory, and absolute paths relative to root,
 * e.g., relative path:  cd csc209/assignment3/
 *       absolute path:  cd /u/bogdan/csc209/assignment3/
 */
int execute_cd(char** words) {
	char *new_dir;
	
	//Check possible errors of the command.
	if (words == NULL || words[0] == NULL || strcmp(words[0], "cd") != 0){
		return EXIT_FAILURE;
	}
	if (words[1] == NULL){
		fprintf(stderr, "cd: Please enter a directory\n");
		return EXIT_FAILURE;
	}

	// First determine if the path is relative or absolute.
	// If it is relative, we append the path to cwd.
	if(is_relative(words[1])){
		if((new_dir = malloc(MAX_DIRNAME)) != NULL){
			getcwd(new_dir, MAX_DIRNAME);
			strcat(new_dir,"/");
		    strcat(new_dir,words[1]);
			if (chdir(new_dir) == -1){
    				perror(words[1]);
				return EXIT_FAILURE;
    		}
		} 
		else {
			fprintf(stderr,"malloc failed!\n");
		}
	} 
	//If it's not relative, then simply change directory to the path.
	else {
		if (chdir(words[1]) == -1){
    		perror(words[1]);
			return EXIT_FAILURE;
    	}
	}
	
	return EXIT_SUCCESS;
}


/**
 * Executes a program, based on the tokens provided as 
 * an argument.
 * For example, "ls -l" is represented in the tokens array by 
 * 2 strings "ls" and "-l", followed by a NULL token.
 * The command "ls -l | wc -l" will contain 5 tokens, 
 * followed by a NULL token. 
 */
int execute_command(char **tokens) {
	char *name = tokens[0];

    if (execvp(name, tokens) == -1){
    	perror(name);
	return EXIT_FAILURE;
    }
	return EXIT_SUCCESS;
}


/**
 * Executes a non-builtin command.
 */
int execute_nonbuiltin(simple_command *s) {
	// Check if the in field is not NULL.
	if(s->in != NULL){
		int fd1;
		
		// Open a new file descriptor.
		if ((fd1 = open(s->in, O_RDONLY, 0777)) == -1){
		    perror("Cannot open input file\n"); 
			return EXIT_FAILURE;
		}
		
		// Redirect stdin/stdout/stderr to the corresponding file.
		if(dup2(fd1, STDIN_FILENO) == -1){
			perror ("dup error 1");
			return EXIT_FAILURE;
		}   
		
		// Close the newly opened file descriptor.
		if (close(fd1) == -1){
			perror ("close error 1");
			return EXIT_FAILURE;
		}
		
	// Check if the out field is not NULL.
	} else if (s->out != NULL) { 
		int fd2;
		
		// Open a new file descriptor.
		if ((fd2 = open(s->out, O_WRONLY|O_TRUNC|O_CREAT, 0777)) == -1){
		    perror("Cannot open output file\n"); 
			return EXIT_FAILURE;
		}
		
		// Redirect stdin/stdout/stderr to the corresponding file.
		if(dup2(fd2, STDOUT_FILENO) == -1){
			perror ("dup error 2");
			return EXIT_FAILURE;
		}    
		
		// Close the newly opened file descriptor.
		if (close(fd2) == -1){
			perror ("close error 2");
			return EXIT_FAILURE;
		}
		
	// Check if the err field is not NULL.
	} else if (s->err != NULL) {
		int fd3;
		
		// Open a new file descriptor.
		if ((fd3 = open(s->err, O_WRONLY|O_TRUNC|O_CREAT, 0777)) == -1){
		    perror("Cannot open output file\n"); 
			return EXIT_FAILURE;
		}
		
		// Redirect stdin/stdout/stderr to the corresponding file.
		if(dup2(fd3, STDERR_FILENO) == -1){
			perror ("dup error 3");
			return EXIT_FAILURE;
		}
		
		// Close the newly opened file descriptor.
		if (close(fd3) == -1){
			perror ("close error 3");
			return EXIT_FAILURE;
		}
	}
	return execute_command(s->tokens);
	
}


/**
 * Executes a simple command (no pipes).
 */
int execute_simple_command(simple_command *cmd) {
	int status, exitcode; 
	
	// Check if the command is builtin.
	// If this is a builtin command.
	if(cmd->builtin != 0){ 
		if (cmd->builtin == BUILTIN_CD) { 
			exitcode = execute_cd(cmd->tokens);
		} 
		else if (cmd->builtin == BUILTIN_EXIT){
			exit(EXIT_SUCCESS);
		}
	} 
	// If this is not a builtin command.
	else {
		// Fork a process to execute the nonbuiltin command.
		switch (fork()){
		case -1:
			perror ("fork");
			return EXIT_FAILURE;
		case 0:
			exit(execute_nonbuiltin(cmd));
		default:
			// The parent should wait for the child.
			wait(&status); 
		}

	}
	return exitcode;
}


/**
 * Executes a complex command.  A complex command is two commands chained 
 * together with a pipe operator.
 */
int execute_complex_command(command *c) {
	int exitcode;

	// Check if this is a simple command.
	// If this is a simple command.
	if(c->scmd != NULL){ 
		exitcode = execute_simple_command(c->scmd);
	} 
	
	// If this is a complex command.
	else {
		if (!strcmp(c->oper, "|")) {
			// Create a pipe that generates a pair of file descriptors.
			int pfd[2];
			if (pipe(pfd) == -1){
				perror ("pipe");
				return EXIT_FAILURE;
			}

			// Fork a new process.
			switch (fork()){
				case -1:
					perror ("fork");
					return EXIT_FAILURE;
			
				// This is the child1.
				case 0:            
					// Close the read end of the pipe.                  
					if (close(pfd[0]) == -1){         
						perror ("close read pipe");
						exit(EXIT_FAILURE);
					} 	
					// Close the stdout file descriptor.
					if(close(STDOUT_FILENO) == -1){
						perror ("close stdout");
						exit(EXIT_FAILURE);
					}
					// Connect the stdout to the write end of the pipe.
					if (pfd[1] != STDOUT_FILENO){
						if(dup2(pfd[1], STDOUT_FILENO) == -1){
							perror ("dup2 1");
							exit(EXIT_FAILURE);
						}
					} 
					// Execute complex command cmd1 recursively. 
					exitcode = execute_complex_command(c->cmd1);     
					exit(exitcode);

				// The parent falls through to create another child process.	
				default:
					break;
			}
			
			// Fork another new process.
			switch (fork()){
				case -1:
					perror ("fork");
					return EXIT_FAILURE;
				// This is the child2.
				case 0:              
					// Close the write end of the pipe.
					if (close(pfd[1]) == -1){    
					perror ("close write pipe");
					exit(EXIT_FAILURE);
					}    
					// Close the stdin file descriptor.
					if(close(STDIN_FILENO) == -1){
						perror ("close stdin");
						exit(EXIT_FAILURE);
					}
					// Connect the stdin to the read end of the pipe.
					if (pfd[0] != STDIN_FILENO){
						if(dup2(pfd[0], STDIN_FILENO) == -1){
							perror ("dup2 2");
							exit(EXIT_FAILURE);
						}
						
					}
					// Execute complex command cmd2 recursively. 
					exitcode = execute_complex_command(c->cmd2);  
					exit(exitcode);

				// The parent falls through.
				default:
					break;
			}
				
			// Close both ends of the pipe. 
			if (close(pfd[0]) == -1){
				perror ("close pipe 0");
				return EXIT_FAILURE;
			}
			if (close(pfd[1]) == -1){
				perror ("close pipe 1");
				return EXIT_FAILURE;
			}
				
			// Wait for both children to finish.
			if (wait(NULL) == -1){
				perror ("wait 1");
				return EXIT_FAILURE;
			}
			if (wait(NULL) == -1){
				perror ("wait 2");
				return EXIT_FAILURE;
			}
		}
	}
	
	return exitcode;
}

