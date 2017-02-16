/*
 * Name: smtpNum
 * Description: STMP user enumeration tool
 * Author: n0vo
 * Date: 02/08/17
 * Compile: gcc -O3 -Wall -g -o smtpNum smtpNum.c -lpthread
 *
 * irc.subhacker.net:6697
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include "main.h"


/* duh */
void help()
{
    puts("Usage: smtpNum <host> <user-file>");
    exit(0);
}

/* handles errors and returns an exit code */
void error(char *msg)
{
    fprintf(stderr, "%s: %s : %d\n", 
			msg, 
			strerror(errno), 
			errno);
    exit(EXIT_FAILURE);
}

/* handler function, returns an exit code */
void death(int sig)
{
    if (sockfd) 
	close(sockfd);
    puts("\nTerminated!");
    exit(0);
}

/* signal trap function, returns a sigaction struct */
int trap(int sig, void(*handler)(int))
{
    struct sigaction action;
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(sig, &action, NULL);
}

int main(int argc, char *argv[])
{
    int		i;			/* thread iterator */
    int		r;			/* thread return code */
    void 	*res;			/* thread results storage */
    pthread_t	tasks[TASK_SIZE];	/* thread identifier */
    int		smtp_code;		/* smtp return code */
    user_t 	args; 			/* args on heap */
    const char 	*filename = argv[2];	/* pointer to file argv */

    if (argc != 3) help();
    /* set options */
    args.ulist = fopen(filename, "r");
    args.host = argv[1];
    args.port = "25";
    /* catch ctrl-C */
    if (trap(SIGINT, death) == -1) 
	error("failed to map handler");
    /* connect and EHLO */
    sockfd = smtp_start(args.host, args.port);
    /* Test for VRFY */
    smtp_report(sockfd, "Testing enumerations methods", 0, 0, 0);
    if ((smtp_code = smtp_test_method(sockfd)) == 0) {
	args.method = 0;
	smtp_report(sockfd, "VRFY was successful", smtp_code, 0, 1);
    } else if (smtp_code == 1) { 
	args.method = 1;
	smtp_report(sockfd, "RCPT TO was successful", smtp_code, 0, 1);
    } else if (smtp_code == 530) { 
	smtp_report(sockfd, "Authentication is required", smtp_code, 2, 1);
    } else {
	smtp_report(sockfd, "All test methods failed", smtp_code, 2, 1);
    }
    /* begin multi-threaded enumeration */
    smtp_report(sockfd, "Starting user enumeration", 0, 0, 0);
    for (args.task_id = 0; args.task_id < TASK_SIZE; ++args.task_id)
        pthread_create(&tasks[args.task_id], NULL, smtp_user_enum, &args);
    /* Join all threads here */
    for (i = 0; i <= TASK_SIZE; i++)
        if ((r = pthread_join(tasks[i], &res)) == 0) return r;

    fclose(args.ulist);
    return EXIT_SUCCESS;
}

