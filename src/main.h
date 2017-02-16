#include <stdio.h>

#define SMTP_CODE 3
#define TASK_SIZE 10
#define REC_SIZE 4096


/* argument structure to be passed to smtp_user_enum() */
typedef struct user_t {
    FILE *ulist;	/* input file */
    int	task_id;	/* task identifier */
    unsigned int meth;	/* 0 for VRFY 1 for RCPT TO -1 for error */
    char *host;		/* host :p */
    const char *port; 	/* bro.. */
    char name[1024];	/* hostname buffer */
} user_t;


/* main socket descriptor */
int sockfd;	
/* error handling */
void error(char *msg);
/* trap handler function */
void death(int sig);
/* handle Ctrl-C */
int trap(int sig, void(*handler)(int));
/* connect to target, ehlo and return socket */
int smtp_start(char *host, const char *port);	
/* sends char *msg to connected int socket and returns recv (the response) */
int smtp_speak(int socket, char *msg);		
/* SMTP user enumeration. This function will be passed to pthread_create */
void *smtp_user_enum(void *info);			
/* Tests which method of enumeration will work */
int smtp_test_method(int socket, char *host);
/* handles SMTP errors and shutdowns */
void smtp_report(int socket, char *msg, int smtp_code, int o_flag, int s_flag);	
