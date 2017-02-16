#include <stdio.h>

#define SMTP_CODE 3 	/*smtp return code */
#define TASK_SIZE 10	/* thread number */
#define REC_SIZE 4096 	/* general message receive size */


/* argument structure to be passed to smtp_user_enum() */
typedef struct user_t {
    FILE *ulist;	/* input file */
    int	task_id;	/* task identifier */
    unsigned int meth;	/* 0 for VRFY 1 for RCPT TO -1 for error */
    char *host;		/* target */
    const char *port; 	/* target port */
    char name[1024];	/* hostname buffer */
} user_t;



int sockfd; 		/* main socket descriptor */
void error(char *msg);	/* error handling */


/*
 * Description:
 *
 * Closes any open sockets and prints "Terminated!"
 * Exists with a success code of 0
 *
 * Usage:
 *
 *    trap(SIGINT, death);
 *
 */
void death(int sig);


/* 
 * Description:
 *
 * Handles Ctrl-C 
 * Takes a signal int and a handler function as arguments
 * We use death() as the handler function in this case
 *
 * Usage:
 *  
 *    if (trap(SIGINT, death) == -1)
 *	  error("failed to map handler"); 
 * 
 * This will catch any SIGINT signal sent by Ctrl-C or otherwise
 * Upon event, death() is executed
 *
 */
int trap(int sig, void(*handler)(int));


/* 
 * Description:
 *
 * This function takes two arguments; host in the form of an IP or hostname, 
 * and a const char port that can be a variable, or a string constant.
 * This function resolves a hostname if necessary, then opens a socket
 * using the info from getaddrinfo. Upon success, it connects to the target
 * sends an EHLO and returns the socket descriptor 
 *
 * Usage:
 *
 *    int sockfd = smtp_start(host, "22");
 *    // sockfd now returns a valid, connected and EHLO'd SMTP connection 
 *
 * The socket open, connect, read, and EHLO routines are all checked for errors
 * So it isn't necessary to do so upon execution of the function
 *
 */
int smtp_start(char *host, const char *port);	


/* 
 * Description:
 * 
 * This function takes a connected socket, and a string as arguments
 * It sends char *msg to int socket and returns recv (the response) 
 * Upon receiving a reponse, the first three chars are copied to a buffer
 * and converted to an int. The int is returned upon completion
 *
 * Usage:
 *
 *    int smtp_code = smtp_speak(sockfd, "VRFY root\r\n");
 *
 * smtp_code now contains a three digit char. 
 * This is the SMTP response code that gets returned upon completion
 *
 */
int smtp_speak(int socket, char *msg);		
 
/*
 * Description:
 * 
 * Handles SMTP errors and shutdowns
 * Takes a scoket, string, smtp code, and two flags as arguments
 * v_flag pertains to the verbosity flag. It can be a 0, 1, or 2
 * 
 * INFO -> 0	// prints [INFO] msg
 * WARNING -> 1 // prints [WARNING] msg : code
 * FATAL -> 2	// prints [FATAL] msg. Exiting with SMTP code: smtp_code
 * 		// FATAL exits with an error code of 2
 *
 * If a socket exists and the s_flag is set then the socket is closed
 *
 * Usage:
 *
 *    smtp_report(sockfd, "Starting enumeration", 0, 0, 0);
 * 
 * Here we are printing an [INFO] message, but doing nothing else
 *
 *    smtp_report(sockfd, "All methods failed", smtp_code, 2, 1);
 *
 * This will print: "[FATAL] All methods failed. Exiting with smtp_code: smtp_code"
 * It will then close the socket and the program with exit with a failure code of 2
 *
 */ 
void smtp_report(int socket, char *msg, int code, int v_flag, int s_flag);	


/* 
 * Description:
 * 
 * Tests which method of enumeration will work on a particular server
 * Function tests:
 *
 * if (VRFY doesn't work)
 *     test MAIL FROM
 *     if (MAIL FROM works)
 *         test RCPT TO
 * if (all fail)
 *    return smtp_code
 *
 * EXPN is excluded because it almost never works anymore
 * Function returns 0 for VRFY, or 1 for RCPT TO
 * Otherwise it returns the SMTP code
 *
 * Usage:
 *    
 *    int smtp_code = smtp_test_method(sockfd, host);
 *    switch (smtp_code) {
 *    case 0:
 *	....
 *
 */
int smtp_test_method(int socket, char *host);


/* 
 * Description:
 *
 * This function takes a user_t struct defined above as an argument
 * This is a void * function as it will be passed to pthread_create
 * It reads from file passed to it by the struct.
 * If the method determined by smtp_test_method is 0, then it tests using VRFY
 * If the moethod is 1, then it uses RCTP TO. Otherwise it exits with FAILURE
 * It then starts a connection and attempts to enumerate one user
 *
 * Usage:
 *
 *    pthread_t tasks[TASK_SIZE];
 *    user_t    args;
 *
 *    for (i = 0; i < TASK_SIZE; i++) 
 *	  pthread_create(&tasks[i], NULL, smtp_user_enum, &args);
 *
 *    for (j = 0; j <= TASK_SIZE; j++)
 *        if ((r = pthread_join(tasks[j], &res)) == 0) return r;
 *
 * This creates 36 threads which all work on the smtp_user_enum function
 * The responses are checked for numerous common STMP errors
 *
 */
void *smtp_user_enum(void *info);			

