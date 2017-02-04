/*
 * Name: smtpNum
 * Description: STMP user enumeration tool
 * Author: n0vo
 * Date: 02/04/17
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
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>

#define RESPONSE 1024


/* argument structure to be passed to worker() */
typedef struct user_t {
    FILE	*ulist;		/* input file */
    int		task_id;	/* task identifier */
    char	*host;		/* target host */
    const char  *port;		/* target port */
} user_t;

void 	error(char *msg); 				/* error function */
void 	death(int sig);		 		    	/* handler function */
int 	trap(int sig, void(*handler)(int));	 	/* signal trap function */
void	*worker(void *info);				/* username vrfy function */
int 	connect_to(char *host, const char *port); 	/* connect to target function */



int	main(int argc, char *argv[])
{
    pthread_t	threads[120];		/* thread identifier */
    int 	r;			/* thread return code */
    void 	*res;			/* thread results storage */
    user_t 	args; 			/* args on heap */
    const char 	*filename = argv[2];	/* pointer to file argv */
    /* initialize the argument structure */
    args.ulist = fopen(filename, "r");
    args.host = argv[1];
    args.port = "25";
    args.task_id = 0;
    /* catch ctrl-C */
    if (trap(SIGINT, death) == -1) error("failed to map handler");
    /* spawn a thread */
    pthread_create(&threads[args.task_id], NULL, worker, &args);
    printf("Worker thread spawned - task id: %d\n", args.task_id);
    /* join the thread results */
    if ((r = pthread_join(threads[args.task_id], &res))) return r;

    fclose(args.ulist);
    return EXIT_SUCCESS;
}

/* handles errors and returns an exit code */
void 	error(char *msg)
{
    fprintf(stderr, "%s: %s : %d\n", 
			msg, 
			strerror(errno), 
			errno);
    exit(EXIT_FAILURE);
}

/* handler function, returns an exit code */
void	death(int sig)
{
//    if (userlist) fclose(userlist);
    puts("\nTerminated!");
    exit(0);
}

/* signal trap function, returns a sigaction struct */
int	trap(int sig, void(*handler)(int))
{
    struct sigaction action;
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(sig, &action, NULL);
}

/* connects to a target server and returns a socket file descriptor */
int 	connect_to(char *host, const char *port)
{
    int			sockfd;		/* socket return code */
    int 		conn;		/* connect return code */
    char		bann[RESPONSE]; /* banner */
    struct addrinfo 	*res;  		/* getaddrinfo result storage */
    struct addrinfo 	hints; 		/* options for getaddrinfo */
    memset(&hints, 0, sizeof(hints));   /* set aside memory for hints stuct */
    hints.ai_family = PF_UNSPEC;	/* unspecified family */
    hints.ai_socktype = SOCK_STREAM;	/* stream socket */
    /* resolve hostname if necessary */
    getaddrinfo(host, port, &hints, &res);
    /* create socket with options from res struct */
    if ((sockfd = socket(res->ai_family, 
			res->ai_socktype,
			res->ai_protocol)) == -1) {
	error("failed to open socket");
    }
    /* make socket asynchronous and non-blocking */
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    fcntl(sockfd, F_SETFL, O_ASYNC);
    /* connect to target */
    if ((conn = connect(sockfd, 
			res->ai_addr, 
			res->ai_addrlen)) == -1) {
	error("failed to connect to target");
    }
    /* receive initial banner */
    recv(sockfd, bann, sizeof(bann), 0);
    freeaddrinfo(res);			/* free the res structure when done */
    return sockfd; 			/* return socket fd */
}

/* test ten users synchronously, to be processed by a thread */
void 	*worker(void *info)
{
    char* c;
    int 	sockfd;		/* socket file descriptor */
    int		sent;		/* bytes sent */
    int		recvd;		/* bytes received */
    char 	user[20];	/* task number */
    char	msg[25];	/* message buffer */
    char	res[RESPONSE]; 	/* response buffer */
    user_t 	*args = info;	/* argument struct */
    /* while file still has lines to read */
    while (fgets(user, sizeof(user), args->ulist)) {
	if ((c = strchr(user, '\n')) != NULL) *c = '\0';
	if ((sockfd = connect_to(args->host, args->port)) == -1)
	    error("failed to grab banner");
        /* create test query */
	snprintf(msg, sizeof(msg), "VRFY %s\n", user);
	/* send it with error checking */
	if ((sent = send(sockfd, 
			    msg, 
			    strlen(msg), 
			    0)) < 0) {
	    error("failed to send test to server");	
	}
	/* receive with error checking */ 
	if ((recvd = read(sockfd,
			    res,
			    sizeof(res))) < 0) {
	    error("failed to receive from server");
        }
	res[recvd] = '\0';
        if (strstr(res, "252") != NULL)
	    printf("%s", res);
        close(sockfd);
    }
    return NULL;
}

