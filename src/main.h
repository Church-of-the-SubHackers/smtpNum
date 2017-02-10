#include <stdio.h>

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


/* main socket descriptor */
int sockfd;	

void error(char *msg);

void death(int sig);

int trap(int sig, void(*handler)(int));

/* connect to target, ehlo and return socket */
int smtp_start(char *host, const char *port);	

/* sends char *msg to connected int socket and returns recv (the response) */
int smtp_speak(int socket, char *msg);		
/* SMTP user enumeration. This function will be passed to pthread_create */
void *smtp_user_enum(void *info);			

/* handles SMTP errors and shutdowns */
void smtp_report(int socket, char *msg, int smtp_code, int o_flag, int s_flag);	
