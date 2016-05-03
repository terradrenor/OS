#define _POSIX_C_SOURCE 201505L

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#define SIZE 2048

void start_line() {
    write(STDOUT_FILENO, "$", 1);
}

void ignore_handler() {
    write(STDOUT_FILENO, "\n", 1);
}

typedef int fd_t;

struct buf_t {
    size_t capacity;
    size_t size;
    char* buf;
};

struct execargs_t {
    char** args;
};

struct buf_t* buf_new(size_t capacity) {
    struct buf_t* buffer = (struct buf_t*) malloc(sizeof(struct buf_t));
    if (!buffer) {
	return NULL;
    }
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->buf = (char*) malloc(capacity);
    if (!buffer->buf) {
	free(buffer);
	return NULL;
    }
    return buffer;
}

struct buf_t* buf_getline(fd_t fd, struct buf_t* buffer, char* dest) {
    size_t current = 0;
    ssize_t ans = 0, read_bytes;
    while (1) {
	if (current == buffer->size) {
	    read_bytes = read(fd, buffer->buf + current, buffer->capacity - current);
	    if (read_bytes == -1)
		return -1;
	    if (read_bytes == 0)
		return 0;
	    buffer->size += read_bytes;
	} else {
	    *dest = buffer->buf[current++];
	    dest++;
	    ans++;
	    if (buffer->buf[current - 1] == '\n') {
		break;
	    }
	}
    }
    memmove(buffer->buf, buffer->buf + current, buffer->size - current);
    buffer->size -= current;
    return ans;
}

void buf_free(struct buf_t* buffer) {
    free(buffer->buf);
    free(buffer);
}

struct execargs_t* create_execargs(char** args, size_t count) {
    struct execargs_t* execargs = (struct execargs_t*) malloc(sizeof(struct execargs_t));
    if (execargs == NULL) {
    	return NULL;
    }
    execargs->args = malloc((count + 1) * sizeof(char*));
    execargs->args[count] = NULL;
    for (size_t i = 0; i < count; i++) {
	execargs->args[i] = strdup(args[i]);
	if (execargs->args[i] == NULL) {
	    for (size_t j = 0; j < i; j++) {
		free(execargs->args[j]);
	    }
	    free(execargs->args);
	    free(execargs);
	    return NULL;
	}
    }
    return execargs;
}

int exec(struct execargs_t* args) {
    return execvp(args->args[0], args->args);
}

pid_t children[SIZE];
size_t count, flag;

void termination_handler() {
    flag = 0;
    for (size_t i = 0; i < count; i++) {
	if (children[i] != 0) {
	    kill(children[i], SIGKILL);
	}
    }
}

int runpiped(struct execargs_t** programs, size_t n) {
    count = n;
    flag = 1;

    //signals
    struct sigaction new_action, old_action;
    new_action.sa_handler = termination_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, &old_action);

    //pipes
    int pipefd[n][2];
    for (size_t i = 0; i < n - 1; i++) {
	if (pipe(pipefd[i]) == -1) {
	    for (size_t j = 0; j < i; j++) {
		close(pipefd[j][0]);
		close(pipefd[j][1]);
	    }
	    return EXIT_FAILURE;
	}
    }
    
    //start
    for (size_t i = 0; i < n; i++) {
	pid_t pid = fork();
	if (pid == -1) {
	    for (size_t j = 0; j < n - 1; j++) {
		close(pipefd[j][0]);
		close(pipefd[j][1]);
	    }
	    termination_handler();
	    return EXIT_FAILURE;
	}
	if (pid == 0) {
	    for (size_t j = 0; j < n - 1; j++) {
		if (j != i - 1) close(pipefd[j][0]);
		if (j != i) close(pipefd[j][1]);
	    }
	    if (i != 0) dup2(pipefd[i - 1][0], STDIN_FILENO);
	    if (i != n - 1) dup2(pipefd[i][1], STDOUT_FILENO);
	    exit(exec(programs[i]) == -1);
	} else {
	    children[i] = pid;
	}
    }

    //closing pipes
    for (size_t i = 0; i < n - 1; i++) {
	close(pipefd[i][0]);
	close(pipefd[i][1]);
    }

    //cancelling sigaction
    int status;
    int ret = 0;
    for (int i = 0; i < n; i++) {
	if (children[i] != 0) {
	    waitpid(children[i], &status, 0);
	    if (WEXITSTATUS(status) != 0) {
		ret = -1;
	    }
	} else {
	    ret = -1;
	}
    }

    sigaction(SIGINT, &old_action, NULL);
    return flag * ret;
}

char str[SIZE];
char* args[SIZE];
struct execargs_t* programs[SIZE];

int main(int arg, char** argv) {
    struct sigaction ignore;
    ignore.sa_handler = ignore_handler;
    sigaction(SIGINT, &ignore, NULL);

    struct buf_t* buffer = buf_new(SIZE);
    ssize_t read_bytes;
    size_t count_args, count_programs, start_str;
    
    while (1) {
        start_line();
        if ((read_bytes = buf_getline(STDOUT_FILENO, buffer, str)) <= 0) {
	    if (read_bytes == 0 || errno != EINTR) {
		break;
	    } else {
		continue;
	    }
	}
	count_args = 0;
	count_programs = 0;
	start_str = 0;
	int was = 0;
	for (size_t i = 0; i < read_bytes; i++) {
	    if (str[i] == '|' || str[i] == '\n') {
		if (was) {
		    str[i] = 0;
		    args[count_args++] = str + start_str;
		}
		programs[count_programs++] = create_execargs(args, count_args);
		was = 0;
		count_args = 0;
	    } else if (str[i] == ' ') {
		if (was) {
		    str[i] = 0;
		    args[count_args++] = str + start_str;
		}
		was = 0;
	    } else {
		if (was != 1) {
		    was = 1;
		    start_str = i;
		}
	    }
	}
	if (runpiped(programs, count_programs) == -1) {
	    write(STDERR_FILENO, "error occured\n", 16);
	}
    }

    buf_free(buffer);
    if (read_bytes == -1) {
	perror("input");
	return EXIT_FAILURE;
    } else {
	return EXIT_SUCCESS;
    }
}
