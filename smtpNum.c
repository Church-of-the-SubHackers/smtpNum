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
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>

#define SMTP_CODE 3
#define TASK_SIZE 10
#define REC_SIZE 4096


/* argument structure to be passed to smtp_user_enum() */
typedef struct user_t {
    FILE *ulist;	 /* input file */
    int	task_id;	 /* task identifier */
    unsigned int method; /* 0 for VRFY, 1 for RCPT TO, -1 for error */
    char *host;
    const char *port;
} user_t;


int 	sockfd;						/* main socket descriptor */
void 	error(char *msg); 				/* general error handling */
void 	death(int sig);		 			/* signal handler function */
int 	trap(int sig, void(*handler)(int));	 	/* signal trap */
int 	smtp_start(char *host, const char *port);	/* connect to target, ehlo and return socket */
int	smtp_speak(int socket, char *msg);		/* sends char *msg to connected int socket and returns recv (the response) */
void	*smtp_user_enum(void *info);			/* SMTP user enumeration. This function will be passed to pthread_create */
void	smtp_bail(int socket, char *msg, int o_flag);	/* handles SMTP errors and shutdowns */



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
    puts("[INFO] Testing VRFY command... ");
    if ((smtp_code = smtp_speak(sockfd, "VRFY\n")) == 501) {
	args.method = 0; /* then VRFY works */
	smtp_bail(sockfd, "[INFO] VRFY was successful", 0);
    } else {
	args.method = -1;
	smtp_bail(sockfd, "[WARNING] VRFY is not allowed on this server.\n", 0);
    }
    /* If VRFY didn't work */
    if (args.method == -1) {
	/* connect again and try RCPT TO */
	sockfd = smtp_start(args.host, args.port);
	puts("[INFO] Testing RCPT TO command... ");
	/* if MAIL FROM was successful */
	if ((smtp_code = smtp_speak(sockfd, "MAIL FROM:test@test.com\n")) == 250) {
	    printf("[INFO] MAIL FROM was successful: %d\n", smtp_code);
	    /* attempt RCPT TO */
	    if ((smtp_code = smtp_speak(sockfd, "RCPT TO:\n")) == 501) { 
		args.method = 1; /* then RCPT TO works */
		smtp_bail(sockfd, "[INFO] RCPT TO was successful", 0);
	    /* Unauthenticated error */
	    } else if (smtp_code == 530) {
		smtp_bail(sockfd, "Authentication is required.", 1);
	    /* All other errors */
	    } else smtp_bail(sockfd, "All test methods failed.", 1);
	/* MAIL FROM didn't succeed */
	} else smtp_bail(sockfd, "All test methods failed.", 1);
    }
    puts("[INFO] Starting SMTP user enumeration...");
    for (args.task_id = 0; args.task_id < TASK_SIZE; ++args.task_id)
        pthread_create(&tasks[args.task_id], NULL, smtp_user_enum, &args);
    /* Join all threads here */
    for (i = 0; i <= TASK_SIZE; i++)
        if ((r = pthread_join(tasks[i], &res)) == 0) return r;

    fclose(args.ulist);
    return EXIT_SUCCESS;
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

/* connects to a target server and returns a socket file descriptor */
int smtp_start(char *host, const char *port)
{
    int			sock;		/* socket return code */
    int 		conn;		/* connect return code */
    char		rec[REC_SIZE];	/* response buffer */
    struct addrinfo 	*res;  		/* getaddrinfo result storage */
    struct addrinfo 	hints; 		/* options for getaddrinfo */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    /* resolve hostname if necessary */
    getaddrinfo(host, port, &hints, &res);
    /* create socket with options from res struct */
    if ((sock = socket(res->ai_family, 
			res->ai_socktype, 
			res->ai_protocol)) == -1) {
	error("failed to open socket");
    }
    /* make socket asynchronous and non-blocking */
    fcntl(sock, F_SETFL, O_NONBLOCK);
    fcntl(sock, F_SETFL, O_ASYNC);
    /* connect to target */
    if ((conn = connect(sock, 
			res->ai_addr, 
			res->ai_addrlen)) == -1) {
	error("failed to connect to server");
    }
    /* receive the banner */
    read(sock, rec, sizeof(rec));
    if ((strstr(rec, "220")) == NULL)
	smtp_bail(sock, "failed to receive banner from server", 1);
    /* send EHLO */
    if ((smtp_speak(sock, "EHLO test\r\n")) != 250)
	smtp_bail(sock, "Failed to send EHLO to server", 1);
    /* finish and return socket */
    freeaddrinfo(res);
    return sock;
}

/* sends a message to a connected socket and return the response */
int smtp_speak(int socket, char *msg)
{
    int  recvd;			/* bytes received */
    int  smtp_code; 		/* smtp response code */
    char receive[REC_SIZE];		/* pointer to response */
    char c[SMTP_CODE];		/* code conversion buffer */

    /* send with error checking */
    if ((send(socket, msg, strlen(msg), 0)) < 0)
        error("failed to send test to server");	
    /* receive with error checking */ 
    if ((recvd = read(socket, receive, sizeof(receive))) < 0)
	error("failed to receive from server");
    receive[recvd] = '\0';
    /* convert the first 3 chars of the response to int */
    strncpy(c, receive, SMTP_CODE);
    smtp_code = atoi(c);
    return smtp_code;
}

/* handles SMTP errors and exits appropriately */
void smtp_bail(int socket, char *msg, int o_flag)
{
    if (socket) close(socket);
    fprintf(stderr, "[ERROR] %s Exiting.\n", msg);
    if (o_flag) exit(2);
}

/* test ten users synchronously, to be processed by a thread */
void *smtp_user_enum(void *info)
{
    char	*c;		/* pointer to newline */
    char 	user[20];	/* task number */
    char	msg[1024];	/* message buffer */
    user_t 	*args = info;	/* argument struct */
   
    while (fgets(user, sizeof(user), args->ulist)) {
	/* strip trailing newline */
	if ((c = strchr(user, '\n')) != NULL) *c = '\0';
	/* determine method of enumeration */
	if (args->method == 0)
             snprintf(msg, sizeof(msg), "VRFY %s\n", user);
	else if (args->method == 1)
	     snprintf(msg, sizeof(msg), "MAIL FROM:test@test.com\r\nRCPT TO:%s\n", user);
	else error("failed to determine method of testing");
	/* connect, EHLO, and begin enumeration */
        int sock = smtp_start(args->host, args->port);
	/*
	 * We should be more thorough with our error checking here
	 * Include checking for 500, 502, 503, 530, and 45* 
	 */
	int smtp_code = smtp_speak(sock, msg);
	switch(smtp_code) {
	case 250:
	    printf("%s : %d\n", user, smtp_code);
	    break;
	case 252:
	    printf("Possible user: %s : %d\n", user, smtp_code);
	    break;
	case 550:
	    break;
	default: smtp_bail(sock, "Unhandled SMTP exception occurred.", 1);
	}
	/* close the socket when done */
        close(sock);
    }
    return NULL;
}

