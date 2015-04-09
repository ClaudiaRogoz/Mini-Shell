/**
 * Operating Sytems 2013 - Assignment 2
 *
 */

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* execute cd */
	int status;
	char err[100];

	/* check home directory */
	if(strcmp(dir->string, "~") == 0){
		return chdir(getenv("HOME"));
	}
	status = chdir(dir->string);

	/* print error message*/
	if(status < 0){
		sprintf(err, "bash: cd: %s: No such file or directory\n", dir->string);
		write(STDERR_FILENO, err, strlen(err));

	}

	return status;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit()
{

	return SHELL_EXIT; 
}

/**
 * Concatenate parts of the word to obtain the command
 */
static char *get_word(word_t *s)
{
	int string_length = 0;
	int substring_length = 0;

	char *string = NULL;
	char *substring = NULL;

	while (s != NULL) {
		substring = strdup(s->string);

		if (substring == NULL) {
			return NULL;
		}

		if (s->expand == true) {
			char *aux = substring;
			substring = getenv(substring);

			/* prevents strlen from failing */
			if (substring == NULL) {
				substring = calloc(1, sizeof(char));
				if (substring == NULL) {
					free(aux);
					return NULL;
				}
			}

			free(aux);
		}

		substring_length = strlen(substring);

		string = realloc(string, string_length + substring_length + 1);
		if (string == NULL) {
			if (substring != NULL)
				free(substring);
			return NULL;
		}

		memset(string + string_length, 0, substring_length + 1);

		strcat(string, substring);
		string_length += substring_length;

		if (s->expand == false) {
			free(substring);
		}

		s = s->next_part;
	}

	return string;
}

/**
 * Concatenate command arguments in a NULL terminated list in order to pass
 * them directly to execv.
 */
static char **get_argv(simple_command_t *command, int *size)
{
	char **argv;
	word_t *param;

	int argc = 0;
	argv = calloc(argc + 1, sizeof(char *));
	assert(argv != NULL);

	argv[argc] = get_word(command->verb);
	assert(argv[argc] != NULL);

	argc++;

	param = command->params;
	while (param != NULL) {
		argv = realloc(argv, (argc + 1) * sizeof(char *));
		assert(argv != NULL);

		argv[argc] = get_word(param);
		assert(argv[argc] != NULL);

		param = param->next_word;
		argc++;
	}

	argv = realloc(argv, (argc + 1) * sizeof(char *));
	assert(argv != NULL);

	argv[argc] = NULL;
	*size = argc;

	return argv;
}
/**
 * redirect stdout to fd_err
 */
int redirect_stderr(simple_command_t *s, int *fd_err, char * file_out, int *fd_out){

	char* file = NULL;
	int rc;

	file = get_word(s->err);

	if (file != NULL){
		/* checks whether file_out and file_err are the same*/
		if(file_out != NULL && strcmp(file_out, file) == 0){

			*fd_err = *fd_out; 		
		}
		/*opens the file based on the flags*/
		else {if(s->io_flags & IO_ERR_APPEND)
			*fd_err = open(file, O_CREAT | O_WRONLY |O_APPEND, 0644);
			else
				*fd_err = open(file, O_CREAT | O_WRONLY |O_TRUNC, 0644);

		}if(*fd_err < 0){
			return EXIT_FAILURE;
		}

		rc = dup2(*fd_err, STDERR_FILENO);

		if(rc < 0){
			return EXIT_FAILURE;
		}
	}

	return 0;
}

/**
 * function that performs redirect from stdout
 * args: s= command; fd_out = file descriptor; file = name of the file 
 * */
int redirect_stdout(simple_command_t *s, int *fd_out, char ** file){

	int rc;	

	*file = get_word(s->out);


	if (*file != NULL){

		/* checking flags in order to open the file */
		if (s->io_flags & IO_OUT_APPEND)
			*fd_out = open(*file, O_WRONLY | O_CREAT | O_APPEND, 0644);
		else 
			*fd_out = open(*file, O_WRONLY | O_CREAT | O_TRUNC , 0644);

		if(*fd_out < 0){
			return EXIT_FAILURE;
		}

		/* redrection from stdout to fd_out */
		rc = dup2(*fd_out, STDOUT_FILENO);

		if(rc < 0){
			return EXIT_FAILURE;
		}
	}
	return 0;
}

/**
 * function that performs redirections from stdin 
 */
int redirect_stdin(simple_command_t *s, int *fd_in){

	char* file = NULL;
	int rc;

	/* finds the last file for the redirection*/
	//for( input = s->in; input!= NULL; input = input->next_word )
	file = get_word(s->in);

	if (file != NULL){

		*fd_in = open(file, O_RDONLY);

		if(*fd_in < 0){
			return EXIT_FAILURE;
		}

		/* redirects stdin to fd_in*/
		rc = dup2(*fd_in, STDIN_FILENO);

		if(rc < 0){
			return EXIT_FAILURE;
		}


	}

	return 0;

}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	char *command;
	const char *var, *val;
	char comm[100];
	char **params;
	int size, rc, pid, status, fd_in = -1, fd_out = -1, fd_err = -1;
	char equal[40];
	char *file = NULL;

	/* sanity checks */
	if(s == NULL)
		return -1;

	/*get command */
	command = get_word(s->verb);

	/* get list of parameters for the command */
	params = get_argv(s, &size);

	/* if builtin command, execute the command */
	if(strcmp(command, "exit") == 0)
		return shell_exit();

	if(strcmp(command, "quit") == 0)
		return shell_exit();
	if (strcmp(command, "cd") == 0){
		rc = shell_cd(s->params);
		if(s->out != NULL && s->out->string != NULL){
			int ff = open(s->out->string, O_CREAT, 0644);
			close(ff);
		}

		return rc;
	}
	/* if variable assignment, execute the assignment and return
	 * the exit status */

	word_t *next = s->verb->next_part;

	if(next != NULL){
		strcpy(equal, next->string);

		/* "=" means variable assignment */
		if(strcmp(equal, "=") == 0){

			/* variable name = var = s->verb->string */
			var = s->verb->string;

			if(next->next_part != NULL){


				/* value = val = next->next_part->string */
				val = next->next_part->string;


			}

			/* set environment variable var = value*/
			rc = setenv(var, val, 1);

			if(rc < 0)
				exit(EXIT_FAILURE);
			return rc;

		}
	}

	/* if external command:
	 *   1. fork new process
	 *     2c. perform redirections in child
	 *     3c. load executable in child
	 *   2. wait for child
	 *   3. return exit status
	 */
	pid = fork();
	switch(pid){
		case -1:
			/* error forking */
			return EXIT_FAILURE;

		case 0:
			/* child process*/

			/* apply redirections if needed*/
			rc = redirect_stdin(s, &fd_in);
			if(rc == EXIT_FAILURE)
				exit(EXIT_FAILURE);
			rc = redirect_stdout(s, &fd_out, &file);
			if(rc == EXIT_FAILURE)
				exit(EXIT_FAILURE);
			rc = redirect_stderr(s, &fd_err, file, &fd_out);
			if(rc == EXIT_FAILURE)
				exit(EXIT_FAILURE);

			/* execute command*/
			rc = execvp(command, (char *const *) params);

			/* in case of error */
			if(rc < 0){

				sprintf(comm, "Execution failed for '%s'\n", command);
				write(STDERR_FILENO, comm, strlen (comm));
				exit(EXIT_FAILURE);
			}
			break;

		default:

			/* parent process */
			if(fd_in >= 0)
				close(fd_in);

			if(fd_out >=0)
				close(fd_out);

			if(fd_err >=0)
				close(fd_err);
			break;
	}	

	/* waits for child */
	waitpid(pid, &status, 0);

	return WEXITSTATUS(status);

}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool do_in_parallel(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* execute cmd1 and cmd2 simultaneously */
	int pid, status;

	pid = fork();
	switch(pid){
		case -1:
			/* error code */
			exit(EXIT_FAILURE);

		case 0:
			/* child process*/
			return parse_command(cmd1, level + 1, father);

		default:
			/* parent process*/
			return parse_command(cmd2, level+1, father);

	}

	/* waits for child*/
	waitpid(pid, &status, 0);

	return WEXITSTATUS(status);
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2)
 */
static bool do_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* redirect the output of cmd1 to the input of cmd2 */

	/* pipe file descriptors*/
	int filedes[2];
	int status, pid;

	/* create pipe */
	status = pipe(filedes);

	if(status == -1)
		exit(EXIT_FAILURE);


	pid = fork();
	switch(pid){
		case -1:
			/* error code */
			exit(EXIT_FAILURE);

		case 0:
			/* first child exec cmd1; writes output to pipe */

			/* read end is unused */
			status = close(filedes[0]);

			if(status == -1)
				exit(EXIT_FAILURE);


			/* redirects stdout to write end of pipe */
			status = dup2(filedes[1], STDOUT_FILENO);

			if(status == -1)
				exit(EXIT_FAILURE);

			status = close(filedes[1]);

			if(status == -1)
				exit(EXIT_FAILURE);

			/* executes command cmd1 */
			if(parse_command(cmd1, level + 1, father) == 0)
				exit(EXIT_SUCCESS);
			else exit(EXIT_FAILURE);

		default:
			break;
	}

	switch(fork()){
		case -1:
			exit(EXIT_FAILURE);

		case 0:
			/* second child exec cmd2; reads input from pipe */

			/* write end is unused */
			status = close(filedes[1]);

			if(status == -1)
				exit(EXIT_FAILURE);

			/* redirects stdin to read end of pipe */
			status = dup2(filedes[0], STDIN_FILENO);

			if(status == -1)
				exit(EXIT_FAILURE);

			status = close(filedes[0]);

			if(status == -1)
				exit(EXIT_FAILURE);

			/* executes command cmd2 */
			if(parse_command(cmd2, level + 1, father) == 0)
				exit(EXIT_SUCCESS);
			else exit(EXIT_FAILURE);

		default:
			break;
	}

	/* parent closes unused file descriptors */
	if(close(filedes[0]) == -1)
		exit(EXIT_FAILURE);

	if(close(filedes[1]) == -1)
		exit(EXIT_FAILURE);

	/* parent wait for its children*/
	if(wait(NULL) == -1){
		exit(EXIT_FAILURE);

	}
	if(wait(NULL) == -1){
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS; /* replace with actual exit status */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* sanity checks */
	if (c == NULL)
		return -1;

	int rc = 0;

	if (c->op == OP_NONE) {
		/* execute a simple command */
		return  parse_simple(c->scmd, level, father);
		/* replace with actual exit code of command */
	}

	switch (c->op) {
		case OP_SEQUENTIAL:
			/* execute the commands one after the other */
			parse_command(c->cmd1, level + 1, father);
			parse_command(c->cmd2, level + 1, father);

			break;

		case OP_PARALLEL:
			/* execute the commands simultaneously */
			return  do_in_parallel(c->cmd1, c->cmd2, level + 1, father);

			break;

		case OP_CONDITIONAL_NZERO:
			/* execute the second command only if the first one
			 * returns non zero */
			rc = parse_command(c->cmd1, level + 1, father);
			if(rc != 0){
				rc = parse_command(c->cmd2, level + 1, father);	
			}
			return rc;

			break;

		case OP_CONDITIONAL_ZERO:
			/* execute the second command only if the first one
			 * returns zero */
			rc = parse_command(c->cmd1, level + 1, father);
			if(rc == 0){
				rc = parse_command(c->cmd2, level + 1, father);	
			}
			return rc;

			break;

		case OP_PIPE:
			/* redirect the output of the first command to the
			 * input of the second */
			return do_on_pipe(c->cmd1, c->cmd2, level + 1, father);

			break;

		default:
			assert(false);
	}

	return rc; /* replace with actual exit code of command */
}

/**
 * Readline from mini-shell.
 */
char *read_line()
{
	char *instr;
	char *chunk;
	char *ret;

	int instr_length;
	int chunk_length;

	int endline = 0;

	instr = NULL;
	instr_length = 0;

	chunk = calloc(CHUNK_SIZE, sizeof(char));
	if (chunk == NULL) {
		fprintf(stderr, ERR_ALLOCATION);
		return instr;
	}

	while (!endline) {
		ret = fgets(chunk, CHUNK_SIZE, stdin);
		if (ret == NULL) {
			break;
		}

		chunk_length = strlen(chunk);
		if (chunk[chunk_length - 1] == '\n') {
			chunk[chunk_length - 1] = 0;
			endline = 1;
		}

		ret = instr;
		instr = realloc(instr, instr_length + CHUNK_SIZE + 1);
		if (instr == NULL) {
			free(ret);
			return instr;
		}
		memset(instr + instr_length, 0, CHUNK_SIZE);
		strcat(instr, chunk);
		instr_length += chunk_length;
	}

	free(chunk);

	return instr;
}
