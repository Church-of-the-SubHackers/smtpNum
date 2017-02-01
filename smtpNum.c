/*
 * Name: smtpNum
 * Description: STMP user enumeration tool
 * Author: n0vo
 * Date: 01/31/16
 * Compile: gcc -O3 -Wall -g -o proxyshell proxyshell.c
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

#define MSG_SIZE 25
#define RESPONSE_SIZE 1024

int sockfd; 						// main socket descriptor
FILE *userlist; 					// username wordlist
void error(char *msg); 					// error function
void handle_shutdown(int sig); 				// handler function
int catch_signal(int sig, void(*handler)(int)); 	// signal handler
int connect_to(char *host, char *port); 		// connect to target



int main(int argc, char *argv[])
{
    char 	test[MSG_SIZE];
    char 	resp[RESPONSE_SIZE];
    char 	*nl, user[20];
    const char 	*filename = argv[2];
    userlist = fopen(filename, "r");

    // connect to the target
    while (fgets(user, sizeof(user), userlist)) {
        sockfd = connect_to(argv[1], "25"); // initialize a socket
        if (catch_signal(SIGINT, handle_shutdown) == -1)
	    error("failed to map handler");
        // receive initial response (banner)
        int recvd = recv(sockfd, resp, sizeof(resp), 0);
        if (recvd == -1) 
	    error("failed to connect to server");
	if ((nl = strchr(user, '\n')) != NULL)
	    *nl = '\0';

	// create the test string
	snprintf(test, sizeof(test), "VRFY %s\n", user);
    	int s = send(sockfd, test, strlen(test), 0);
    	if (s == -1)
	    error("failed to send to server");

    	// Receive the response
    	recvd = recv(sockfd, resp, sizeof(resp), 0);
    	resp[recvd] = '\0'; // make it a proper string

    	// print the response    
	if (strstr(resp, "550") == NULL)
	    printf("%s", resp);
	close(sockfd);
    }
    fclose(userlist);

    return EXIT_SUCCESS;
}

void error(char *msg)
{
    fprintf(stderr, "%s: %s code: %d\n", msg, strerror(errno), errno);
    exit(1);
}

void handle_shutdown(int sig)
{
    if (sockfd) 
	close(sockfd);
    if (userlist) 
	fclose(userlist);

    puts("\nTerminated!");
    exit(0);
}

int catch_signal(int sig, void(*handler)(int))
{
    struct sigaction action;
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(sig, &action, NULL);
}

int connect_to(char *host, char *port)
{
    struct addrinfo *res;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(host, port, &hints, &res);
    // create socket with options from res struct
    int sock = socket(res->ai_family, res->ai_socktype,
			res->ai_protocol);
    if (sock == -1)
	error("failed to open socket");
    // connect to target
    int conn = connect(sock, res->ai_addr, res->ai_addrlen);
    if (conn == -1)
	error("failed to connect to target");
    // free the res structure when done with it
    freeaddrinfo(res); 

    return sock; // returns a socket file descriptor
}

