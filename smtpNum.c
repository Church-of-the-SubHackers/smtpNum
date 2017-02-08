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


int 	sockfd;							/* main socket descriptor */
void 	error(char *msg); 					/* general error handling */
void 	death(int sig);		 		    		/* signal handler function */
int 	trap(int sig, void(*handler)(int));	 		/* signal trap */
int 	smtp_start(char *host, const char *port); 		/* connect to target, ehlo and return socket */
int	smtp_code(char *response, const char *code);		/* returns 0 if code is found in response, otherwise returns -1 */
void	smtp_speak(int socket, const char *msg, char *recv);	/* sends char *msg to connected int socket and returns recv (the response) */
void	*smtp_user_enum(void *info);				/* SMTP user enumeration. This function will be passed to pthread_create */
void	smtp_bail(int socket, char *msg);			/* handles SMTP errors and shutdowns */



int main(int argc, char *argv[])
{
    int		i;			/* thread iterator */
    int		r;			/* thread return code */
    void 	*res;			/* thread results storage */
    pthread_t	threads[TASK_SIZE];	/* thread identifier */
    char	receive[REC_SIZE];	/* response buffer */
    user_t 	args; 			/* args on heap */
    const char 	*filename = argv[2];	/* pointer to file argv */
    args.ulist = fopen(filename, "r");	/* open file for reading */
    args.host = argv[1];	/* command line argument 1 for host */
    args.port = "25";		/* default port 25 */
    /* catch ctrl-C */
    if (trap(SIGINT, death) == -1) error("failed to map handler");
    /* connect and EHLO */
    sockfd = smtp_start(args.host, args.port);
    /* Test for VRFY */
    puts("[INFO] Testing VRFY command... ");
    smtp_speak(sockfd, "VRFY\n", receive);
    if ((smtp_code(receive, "501")) == 0) {
	puts("[INFO] VRFY was successful");
	args.method = 0; /* then VRFY works */
	close(sockfd);
    } else {
	fprintf(stderr, "[WARNING] VRFY is not allowed on this server.\n");
	args.method = -1;
	close(sockfd);
    }
    /* If VRFY didn't work */
    if (args.method == -1) {
	/* connect again and try RCPT TO */
	sockfd = smtp_start(args.host, args.port);
	puts("[INFO] Testing RCPT TO command... ");
	smtp_speak(sockfd, "MAIL FROM:test@test.com\n", receive);
	/* if MAIL FROM was successful */
	if ((smtp_code(receive, "250")) == 0) {
	    /* attempt RCPT TO */
	    smtp_speak(sockfd, "RCPT TO:\n", receive);
	    /* a syntax error means the command is allowed, we just didn't send any parameters */
	    if ((smtp_code(receive, "501")) == 0) { 
		puts("[INFO] RCPT TO was successful");
		args.method = 1; /* then RCPT TO works */
		close(sockfd);
	    /* Unauthenticated error */
	    } else if ((smtp_code(receive, "530")) == 0)
		smtp_bail(sockfd, "Authentication is required.");
	    else
	        smtp_bail(sockfd, "All test methods failed.");
	} else
	    smtp_bail(sockfd, "All test methods failed.");
    }
    puts("[INFO] Starting SMTP user enumeration...");
    for (args.task_id = 0; args.task_id < TASK_SIZE; ++args.task_id)
        pthread_create(&threads[args.task_id], NULL, smtp_user_enum, &args);
    /* Join all threads here */
    for (i = 0; i <= TASK_SIZE; i++)
        if ((r = pthread_join(threads[i], &res)) == 0) return r;

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
    char		rec[REC_SIZE];
    struct addrinfo 	*res;  		/* getaddrinfo result storage */
    struct addrinfo 	hints; 		/* options for getaddrinfo */
    memset(&hints, 0, sizeof(hints));   /* set aside memory for hints stuct */
    hints.ai_family = PF_UNSPEC;	/* unspecified family */
    hints.ai_socktype = SOCK_STREAM;	/* stream socket */
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
    if ((smtp_code(rec, "220")) == -1)
	error("failed to receive banner from server");
    /* send EHLO */
    smtp_speak(sock, "EHLO test\n", rec);
    if ((smtp_code(rec, "250")) == -1)
	error("failed to send data to server");
    /* receive EHLO response before moving on */
    read(sock, rec, sizeof(rec));
    /* finish and return socket */
    freeaddrinfo(res);
    return sock;
}

int smtp_code(char *response, const char *code)
{
    char c[SMTP_CODE];
    strncpy(c, response, SMTP_CODE);
    if ((strncmp(code, c, SMTP_CODE)) == 0) {
        return 0;
    } else return -1;
}

/* sends a message to a connected socket and return the response */
void smtp_speak(int socket, const char *msg, char *receive)
{
    int	 sent;		/* bytes sent */
    int	 recvd;		/* bytes received */
    /* send with error checking */
    if ((sent = send(socket, msg, strlen(msg), 0)) < 0)
        error("failed to send test to server");	
    /* receive with error checking */ 
    if ((recvd = read(socket, receive, sizeof(receive))) < 0)
	error("failed to receive from server");
    receive[recvd] = '\0';
}

/* handles SMTP errors and exits appropriately */
void smtp_bail(int socket, char *msg)
{
    if (socket) close(socket);
    fprintf(stderr, "[ERROR] %s Exiting.\n", msg);
    exit(2);
}

/* test ten users synchronously, to be processed by a thread */
void *smtp_user_enum(void *info)
{
    int sock;
    char	*c;		/* pointer to newline */
    char 	user[20];	/* task number */
    char	msg[25];	/* message buffer */
    char	rec[REC_SIZE];	/* response buffer */
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
        sock = smtp_start(args->host, args->port);
	smtp_speak(sock, msg, rec);
	if ((smtp_code(rec, "250")) == 0)
	    printf("%s", user);
	else if ((smtp_code(rec, "252")) == 0)
	    printf("Possible user: %s\n", user);
	else if ((smtp_code(rec, "550")) == 0);
	else smtp_bail(sock, "Unknown SMTP exception occurred.");
	/* close the socket when done */
        close(sock);
    }
    return NULL;
}

