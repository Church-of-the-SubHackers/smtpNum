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
    if (sockfd) close(sockfd);
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
    smtp_report(sockfd, "Testing VRFY", 0, 0, 0);
    if ((smtp_code = smtp_speak(sockfd, "VRFY\n")) == 501) {
	args.method = 0; /* then VRFY works */
	smtp_report(sockfd, "VRFY was successful", smtp_code, 0, 1);
    } else {
	args.method = -1;
	smtp_report(sockfd, "VRFY is not allowed on this server", smtp_code, 1, 1);
    }
    /* If VRFY didn't work */
    if (args.method == -1) {
	/* connect again and try RCPT TO */
	sockfd = smtp_start(args.host, args.port);
	smtp_report(sockfd, "Testing RCPT TO command", 0, 0, 0);
	/* if MAIL FROM was successful */
	if ((smtp_code = smtp_speak(sockfd, "MAIL FROM:test@test.com\n")) == 250) {
	    smtp_report(sockfd, "MAIL FROM was successful", smtp_code, 0, 0);
	    /* attempt RCPT TO */
	    if ((smtp_code = smtp_speak(sockfd, "RCPT TO:test\n")) == 550) { 
		args.method = 1; /* then RCPT TO works */
		smtp_report(sockfd, "RCPT TO was successful", smtp_code, 0, 1);
	    /* Unauthenticated error */
	    } else if (smtp_code == 530) {
		smtp_report(sockfd, "Authentication is required", smtp_code, 2, 1);
	    /* All other errors */
	    } else 
		smtp_report(sockfd, "All test methods failed", smtp_code, 2, 1);
	/* MAIL FROM didn't succeed */
	} else 
	    smtp_report(sockfd, "All test methods failed", smtp_code, 2, 1);
    }
    smtp_report(sockfd, "Starting user enumeration", 0, 0, 0);
    for (args.task_id = 0; args.task_id < TASK_SIZE; ++args.task_id)
        pthread_create(&tasks[args.task_id], NULL, smtp_user_enum, &args);
    /* Join all threads here */
    for (i = 0; i <= TASK_SIZE; i++)
        if ((r = pthread_join(tasks[i], &res)) == 0) return r;

    fclose(args.ulist);
    return EXIT_SUCCESS;
}

