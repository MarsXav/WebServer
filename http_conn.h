#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <bits/stdc++.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stat.h>
#include <sys/mmap.h>
#include <errno.h>
#include <fcntl.h>
#include "locker.h"
#include <sys/uio.h>


class http_conn{
public:
	static int m_epollfd; //all events will be registered to the same epoll entity
	static int m_user_count; //count all users

	http_conn(){}
	~http_conn(){}

	void process(); // handle client's requests

	void init(int sockfd, const sockaddr_in &addr); // initialise new connected clients

	void close_conn(); //close connection

	bool read(); //nonblocking write
	bool write(); //nonblocking write

private:
	int m_sockfd; // the HTTP socket connects;
	sockaddr_in m_address; // socket adderess used for communication


}

#endif
