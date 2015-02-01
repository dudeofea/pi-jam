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
#include <pthread.h>
#include "jack_client.h"

typedef struct
{
	int index;
	int size;
	pthread_t* thread_ids;
	int* newsockfds;
} thread_pool;

thread_pool tp;
int sockfd;

int doprocessing(int sock);

void* handle_connection(void* args){
	int* sock = (int*)args;
	int r = doprocessing(*sock);
	stop_pijam();
	for(int i = 0; i < tp.index; i++){
		close(tp.newsockfds[i]);
	}
	close(sockfd);
	shutdown(sockfd, 2);
	exit(r);
}

int main( int argc, char *argv[])
{
	//Start pijam
	start_pijam();

	int newsockfd, portno, clilen;
	struct sockaddr_in serv_addr, cli_addr;

	// Init thread pool
	tp.index = 0;
	tp.size = 10;
	tp.thread_ids = (pthread_t*)malloc(tp.size*sizeof(pthread_t));
	tp.newsockfds = (int*)malloc(tp.size*sizeof(int));

	//First call to socket() function
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0){
		perror("ERROR opening socket");
		exit(1);
	}

	// Initialize socket structure
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = 5002;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	// Now bind the host address using bind() call.
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		perror("ERROR on binding");
		exit(1);
	}

	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	
	while (1)
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t * restrict) &clilen);
		if (newsockfd < 0){
			perror("ERROR on accept");
			exit(1);
		}

		// Create child process
		tp.newsockfds[tp.index] = newsockfd;
		if(pthread_create(&tp.thread_ids[tp.index], NULL, handle_connection, &tp.newsockfds[tp.index])){
			printf("error spawning thread\n");
		}
		tp.index++;
		if(tp.index >= tp.size){
			//realloc
			tp.size *= 2;
			tp.thread_ids = (pthread_t*)realloc(tp.thread_ids, tp.size * sizeof(pthread_t));
			tp.newsockfds = (int*)realloc(tp.newsockfds, tp.size * sizeof(int));
		}
		//close(newsockfd);
	}
	stop_pijam();
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
		play_sample(instrument, extra);
		//printf("inst: %d, extra: %d\n", instrument, extra);

		if (n < 0){
			perror("ERROR reading from socket");
			return 1;
		}

		//printf("Here is the message: %s\n",buffer);
		//n = write(sock,"I got your message",18);

		if (n < 0){
			perror("ERROR writing to socket");
			return 1;
		}
	}
	return 1;
}
