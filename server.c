/*
*
*	Server to accept incoming connections and
*	generate music at request
*
*	Socket code taken mostly from http://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "jack_client.h"

int doprocessing(int sock);

int main( int argc, char *argv[] )
{
	//Start pijam
	start_pijam();

	int sockfd, newsockfd, portno, clilen;
	struct sockaddr_in serv_addr, cli_addr;
	int  pid;

	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0){
		perror("ERROR opening socket");
		exit(1);
	}

	/* Initialize socket structure */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = 5001;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		perror("ERROR on binding");
		exit(1);
	}

	/* Now start listening for the clients, here
	* process will go in sleep mode and will wait
	* for the incoming connection
	*/

	listen(sockfd,5);
	clilen = sizeof(cli_addr);

	while (1)
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t * restrict) &clilen);
		if (newsockfd < 0){
			perror("ERROR on accept");
			exit(1);
		}

		/* Create child process */
		pid = fork();
		if (pid < 0){
			perror("ERROR on fork");
			exit(1);
		}

		if (pid == 0){
			/* This is the client process */
			close(sockfd);
			int r = doprocessing(newsockfd);
			stop_pijam();
			exit(r);
		}
		else{
			close(newsockfd);
		}
	} /* end of while */
}

int doprocessing (int sock)
{
	int n;
	char buffer[256];

	while(1){
		bzero(buffer,256);

		n = read(sock,buffer,255);
		int instrument, extra;
		sscanf(buffer, "%d,%d", &instrument, &extra);

		if(instrument < 0){
			//this is the exit code
			return 0;
		}
		printf("inst: %d, extra: %d\n", instrument, extra);

		if (n < 0){
			perror("ERROR reading from socket");
			return 1;
		}

		printf("Here is the message: %s\n",buffer);
		n = write(sock,"I got your message",18);

		if (n < 0){
			perror("ERROR writing to socket");
			return 1;
		}
	}
	return 1;
}
