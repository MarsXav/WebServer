#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535 //maximum number of fds
#define MAX_EVENT_NUM 1000

// register signal capture
void addsig(int sig, void(handler)(int)){
	struct sigaction sa; // parameter of registration of signal
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler; 
	sigfillset(&sa.sa_mask);
	sigaction(sig, &sa, NULL);
}

// add fd to epoll
extern void addfd(int epollfd, int fd, bool one_shot);
// delete fd from epoll
extern void removefd(int epollfd, int fd);
// mofidy fd from epoll
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char* argv[]){ 
	if (argc <= 1){
		printf("Please input command as this: %s port number\n", basename(argv[0]));
		exit(-1);
	}
	// obtain the port number
	int port = atoi(argv[1]);

	// handle SIGPIPE signal
	addsig(SIGPIPE, SIG_IGN);

	
	// create a thread pool, initialise thread pool
	threadpool<http_conn> *pool = NULL;
	try{
		pool = new threadpool<http_conn>;
	} catch(...){
		exit(-1);
	}

	// create an array to store all client's information
	http_conn *users = new http_conn[MAX_FD];
	
	// create a socket 
	int listenfd = socked(PF_INET, SOCK_STREAM, 0);

	// set port reuse
	int reuse = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	//bind
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	bind(listenfd, (struct sockaddr*)&address, sizeof(address));

	//listen
	listen(listenfd, 5);
	
	// create an epoll entity, event array
	epoll_event event[MAX_EVENT_NUM];

	int epollfd = epoll_create(5);

	// add the listened fds to the epoll entities
        addfd(epollfd, listenfd, false);

	http_conn::m_epollfd = epollfd;

	while(1){ // main thread detects if there are events happening
		int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if (num < 0 and errno != EINTR){
			printf("epoll fails\n");
			break;
		} 
		// traverse event array
		for (int i =0 ; i < num; i++){
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd){
				// clients connects
				struct sockaddr_in client_address;
				socklen_t client_addrlen = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);
				
				if (http_conn::m_user_count >= MAX_FD){
					close(connfd);
					continue;
				}
				// initialise client's data, put them into the array
				users[connfd] .init(connfd, client_address);
				

			} else if (events[i].events & (EPOLLRDHUP | EPOLL HUP | EPOLLERR)){
				// disconnet or error events
				users[sockfd].close_conn();
			} else if (events[i].events & EPOLLIN){
				if (users[sockfd].read()) { //read all data
					pool -> append(users + sockfd);
				} else {
					users[sockfd].close_conn();
				}
			} else if (events[i].events & EPOLLOUT){
				if (!users[sockfd].write()) { // write all data
					users[sockfd].close_conn();
				}
			}
		}
	}
	close(epollfd);
	close(listenfd);
	delete [] users;
	delete [] pool;

	


	return 0;
}
