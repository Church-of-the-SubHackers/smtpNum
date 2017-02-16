#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>


#include "main.h"


/* 
 * resolves a hostname if necessary, and opens a socket 
 * using the info from getaddrinfo
 *
 * Upon success, it connects to the host on the port specified
 * If there is a banner, the function receives it and sends an EHLO
 * If all was successful, it frees up the res structure and returns the socket
 */
int smtp_start(char *host, const char *port)
{
    int			sock;		/* socket return code */
    int 		conn;		/* connect return code */
    int			smtp_code;	/* smtp return code */
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
    /* connect to target */
    if ((conn = connect(sock, 
			res->ai_addr, 
			res->ai_addrlen)) == -1) {
	error("failed to connect to server");
    }
    /* receive the banner */
    read(sock, rec, sizeof(rec));
    if ((strstr(rec, "220")) == NULL)
	smtp_report(sock, "failed to receive banner from server", 2, 2, 1);
    /* send EHLO */
    if ((smtp_code = smtp_speak(sock, "EHLO test\r\n")) != 250)
	smtp_report(sock, "Failed to send EHLO to server", smtp_code, 2, 1);
    /* finish and return socket */
    freeaddrinfo(res);
    return sock;
}

/* 
 * sends a message to a connected socket passed to the function
 * and receives the response, adding a null terminator for proper format
 * 
 * the first three characters of the response are copied into a buffer
 * and converted to an integer
 * function returns the integer upon completion
 */
int smtp_speak(int socket, char *msg)
{
    int  recvd;			/* bytes received */
    int  smtp_code; 		/* smtp response code */
    char receive[REC_SIZE];	/* pointer to response */
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
/* This may be handled by an ifdef soon */
void smtp_report(int socket, char *msg, int code, int o_flag, int s_flag)
{
    if (s_flag && socket) close(socket);
    /* o_flag decides the level of urgency */
    switch(o_flag) {
    case 0:
	fprintf(stderr, "[INFO] %s\n", msg);
	break;
    case 1:
	fprintf(stderr, "[WARNING] %s : %d\n", msg, code);
	break;
    case 2:
	fprintf(stderr, "[FATAL] %s. Exiting with SMTP code: %d\n", msg, code);
	exit(2);
	break;
    default: error("invalid flag");
    }
}

/*
 * this function determines which method of SMTP enumeration will work against
 * a specified target, if any
 *
 * function will test:
 *
 *  VRFY
 *  MAIL FROM, RCPT TO
 *
 * EXPN will be excluded due to scarcity of success
 *
 * function will return 0 for VRFY, 1 for RCPT TO, or -1 if all fail
 */
int smtp_test_method(int socket, char *host)
{
    int  smtp_code;
    char msg[512];

    /* If VRFY doesn't work */
    if ((smtp_code = smtp_speak(socket, "VRFY root\r\n")) != 501 || 
	smtp_code != 250) {
	// VERBOSE ifdef
	snprintf(msg, sizeof(msg), "MAIL FROM:num@num.com\r\n");
	/* test MAIL FROM */
	if ((smtp_code = smtp_speak(socket, msg)) == 250) {
	    /* if it works then test RCPT TO */
	    // VERBOSE ifdef
	    snprintf(msg, sizeof(msg), "RCPT TO:root\r\n");
	    if ((smtp_code = smtp_speak(socket, msg)) == 250 || 
		smtp_code == 550) {
	        return 1; /* RCPT TO works */
	    } else return smtp_code;
	} else return smtp_code;
    } else smtp_code = 0;

    return smtp_code;
}

/* 
 * function takes a user_t structure defined in main.h as an argument
 * function reads usernames from a file line by line
 * it determines the test method based on args->method (from the struct) 
 *
 * once determined, function starts an smtp session and begins testing
 * function tests one user at a time
 * SMTP return code checking is implementend an handles various common codes
 * different outputs occur depending on the return code
 * function returns NULL as it is a (void *) to be passed to pthread_create
 */
void *smtp_user_enum(void *info)
{
    int		sock;		/* socket file descriptor */
    char	*c;		/* pointer to newline */
    char 	user[20];	/* username from file */
    char	msg[1024];	/* message buffer */
    int		err;		/* keeps count of errors */
    user_t 	*args = info;	/* argument struct */
   
    err = 0;
    while (fgets(user, sizeof(user), args->ulist)) {
	/* strip trailing newline */
	if ((c = strchr(user, '\n')) != NULL) *c = '\0';
	/* determine method of enumeration */
	if (args->meth == 0) {
             snprintf(msg, sizeof(msg), "VRFY %s\n", user);
	} else if (args->meth == 1) {
	    if ((strcmp(args->name, "")) == 0)
		snprintf(msg, sizeof(msg), "RCPT TO:%s\n", user);
	    else 
		snprintf(msg, sizeof(msg), "RCPT TO:%s@%s\n", user, args->name);

	} else error("failed to determine method of testing");
	/* connect, EHLO, and begin enumeration */
        sock = smtp_start(args->host, args->port);
	/* send an initial MAIL FROM */
	if (args->meth == 1) 
	    smtp_speak(sock, "MAIL FROM:num@num.com\r\n");
	int smtp_code = smtp_speak(sock, msg);
	switch(smtp_code) {
	case 250:
	    printf("%s\n", user);
	    break;
	case 252:
	    smtp_report(sock, "VRFY disallowed", smtp_code, 2, 1);
	    break;
	case 450:
	case 451:
	case 452:
	    smtp_report(sock, "Requested action failed", smtp_code, 2, 1);
	    break;
	case 500:
	case 501:
	    smtp_report(sock, "Syntax error", smtp_code, 1, 1);
	    if (err == 1)
		smtp_report(sock, "Too many errors", smtp_code, 2, 1);
	    ++err;
	    break;
	case 502:
	    smtp_report(sock, "Command is not allowed", smtp_code, 2, 1);
	    break;
	case 503:
	    smtp_report(sock, "Bad command sequence, or auth required", 
	    smtp_code, 2, 1);
	    break;
	case 530:
	    smtp_report(sock, "Authentication required", smtp_code, 2, 1);
	    break;
	case 550:
	    break;
	default: 
	    smtp_report(sock, "Unhandled SMTP exception", smtp_code, 2, 1);
	}
	/* close the socket when done */
        close(sock);
    }
    return NULL;
}

