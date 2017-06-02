/*
 * Author: Sam Oberg
 * Duckid: 951260070
 * Title: CIS 415 Project 1
 * 
 * All of this code was done by me with the following exceptions:
 * Brian Leeson helped be with ignoring other signals in my CHLD_handler
 *
 * Some of the code was directily copied from the LPE pdf 
 * (how to wait for children to terminate, how to set an alarm, how to handle those signals)
 *
 * My use of nanosleep and my USR1 handler is copied from Piazza.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include "p1fxns.h"
#include <time.h>

#define PAUSED 2
#define RUNNING 1
#define DEAD 0
#define UNUSED __attribute__((unused))

struct _queue;
typedef struct _queue Queue;
struct _process;
typedef struct _process Process;

int USR1_received = 0;
int active_processes;

Process *curProc;
Queue *queue;

struct _process {
    struct _process *next; 
    int status;
    pid_t pid;
    char *cmd;
    char **args;
};

struct _queue {
    Process *head;
    Process *tail;
};

void remove_newline_char(char *str) {
    int i;
    if ((i = p1strchr(str, '\n')) == -1) {
        //newline char not present
        return;
    }
    str[i] = '\0';
}

Process *dequeue() {
    Process *retval = queue->head;
    if (retval != NULL) {
        queue->head = retval->next; 
    }
    return retval;
}

void enqueue(Process *proc) {
    if (queue->head == NULL) {
        queue->head = proc;
    } else {
        queue->tail->next = proc;
    }
    queue->tail = proc;
    queue->tail->next = NULL;
}

static void onalarm(UNUSED int sig) {
    signal(SIGINT, SIG_IGN);
    kill(curProc->pid, SIGSTOP);
    enqueue(curProc);

    do { //get the next ready process that isn't dead or null
        curProc = dequeue();
    } while ( (curProc != NULL) && (curProc->status == DEAD) );

    if (curProc != NULL) { 
        if (curProc->status == PAUSED) {
            curProc->status = RUNNING;
            kill(curProc->pid, SIGUSR1);
        } else {
            kill(curProc->pid, SIGCONT);
        }
    }
    signal(SIGINT, SIG_DFL);
}

/* from book */
static void CHLD_handler(UNUSED int sig) {
    signal(SIGINT, SIG_IGN);
    pid_t pid;
    int status;
    Process *cur = curProc;

    /* Wait for all dead processes.
    * We use a non-blocking call to be sure this signal handler will not
    * block if a child was cleaned up in another part of the program. */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            active_processes--;
            while (cur != NULL) {
                if (cur->pid == pid) {
                    cur->status = DEAD;
                    break;
                }
                cur = cur->next; 
            }
        }
    }
    signal(SIGINT, SIG_DFL);
}

static void USR1_handler(int sig) {
    if (sig == SIGUSR1) {
        USR1_received++;
    }
    return;
}

void demalloc_processes(Process **procs, int numProcs) {
    int i, j;
    
    for (i = 0; i < numProcs; i++) { 
        j = 0;
        char *arg = procs[i]->args[j];
        while (arg != NULL) {
            free(arg);
            arg = procs[i]->args[++j];
        }
        free(procs[i]->args);
        free(procs[i]->cmd);
        free(procs[i]);
    }
    free(procs);
}

void subscribe_to_signals() {
    if (signal(SIGCHLD, CHLD_handler) == SIG_ERR) {
        p1perror(2, "Can't establish SIGCHLD handler\n");
        exit(1);
    }

    if (signal(SIGUSR1, USR1_handler) == SIG_ERR) {
        p1perror(2, "Can't establish SIGUSR1 handler\n");
        exit(1);
    }
}

void set_timer(int quantum) {
    struct itimerval it_val; /* for setting itimer */
    /* Upon SIGALRM, call onalarm().
    * Set interval timer. We want frequency in ms,
    * but the setitimer call needs seconds and useconds. */
    if (signal(SIGALRM, onalarm) == SIG_ERR) {
        p1perror(2, "Unable to catch SIGALRM");
        exit(1);
    }

    it_val.it_value.tv_sec = quantum/1000;
    it_val.it_value.tv_usec = (quantum*1000) % 1000000;
    it_val.it_interval = it_val.it_value;

    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        p1perror(2, "error calling setitimer()");
        exit(1); 
    }
}

void init_queue() {
    if ((queue = (Queue *) malloc(sizeof(Queue))) == NULL) {
        p1perror(2, "failed malloc");
        exit(1);
    }
    queue->head = NULL;
    queue->tail = NULL;
}

void parse_args(int argc, char *argv[], int *quantum, char *file, int *fd) {
    char *p;
    int index;

    if ((p = getenv("USPS_QUANTUM_MSEC")) != NULL) {
        *quantum = p1atoi(p);
    }
 
    if (argc > 1) {
        if (p1strneq(argv[1], "--quantum=", 10)) {
            index = p1strchr(argv[1], '=');
            *quantum = p1atoi(&argv[1][index+1]);
        } else {
            file = argv[1];
        }
    } 
    if (argc > 2) {
        if (p1strneq(argv[2], "--quantum=", 10)) {
            index = p1strchr(argv[2], '=');
            *quantum = p1atoi(&argv[2][index+1]);
        } else {
            file = argv[2];
        }
    }

    if (-1 == *quantum) {
        p1perror(2, "error: quantum not specified");
        exit(1);
    }

    if (NULL != file) {
        if ((*fd = open(file, 0)) < 0) {
            p1perror(2, "error: file not found");
            exit(1);
        }
    } else {
        //so get from stdin
        *fd = 0;
    }
}

void get_processes(int *numProcesses, int fd) {
    char buf[512];
    char word[512];
    int index;
    int numArgs;
    Process *cur = NULL;
    char **args;

    
    while (p1getline(fd, buf, sizeof(buf)) > 0) {
        //must count number of args before creating arg array
        index = 0;
        numArgs = -1;
        while (index >= 0) {
            numArgs++;
            index = p1getword(buf, index, word);
        }

        if ((cur = (Process *) malloc(sizeof(Process))) == NULL) {
            p1perror(2, "failed malloc");
            exit(1);
        }

        if ((args = (char **) malloc(sizeof(char *) * (numArgs + 1))) == NULL) {
            p1perror(2, "failed malloc");
            exit(1);
        }

        //now actually copy all of the strings to cmd and args
        index = p1getword(buf, 0, word);

        remove_newline_char(word);
        cur->cmd = p1strdup(word);
        args[0] = p1strdup(word);

        int j = 1;
        while (j < numArgs) {
            index = p1getword(buf, index, word);
            remove_newline_char(word);
            args[j] = p1strdup(word);
            j++;
        }

        args[j] = NULL;
        cur->args = args;
        cur->next = NULL;
        enqueue(cur);
        (*numProcesses)++;
    }
}


int main(int argc, char *argv[]) { 
    struct timespec tm = {0, 20000000};	/* 20,000,000 ns == 20 ms */
    int quantum = -1;
    int index;
    char *file = NULL;
    int fd;
    int numProcesses = 0;
    Process **procs;
    Process *cur;

    parse_args(argc, argv, &quantum, file, &fd);
    init_queue();
    get_processes(&numProcesses, fd);
    subscribe_to_signals();

    if ((procs = (Process **) malloc(sizeof(Process *) * numProcesses)) == NULL) {
        p1perror(2, "failed malloc");
        exit(1);
    }

    cur = queue->head;
    for (index = 0; index < numProcesses; index++) {
        procs[index] = cur;
        procs[index]->status = PAUSED;
        procs[index]->pid = fork();
        if (procs[index]->pid == 0) { //I am a child process
            while(!USR1_received) {
                (void)nanosleep(&tm, NULL);
            }
            execvp(cur->cmd, cur->args);
            p1perror(2, "bad command");
            exit(1);
        } else if (procs[index]->pid < 0) {
            p1perror(2, "failed fork");
            exit(1);
        }       
        cur = cur->next;
    }

    active_processes = numProcesses;

    //start the very first process
    curProc = dequeue();
    if (curProc != NULL) {
        curProc->status = RUNNING;
        kill(curProc->pid, SIGUSR1);
    }

    set_timer(quantum);

    while (active_processes) {
        pause();
    }

    free(queue);
    demalloc_processes(procs, numProcesses);
    return 0;
}
