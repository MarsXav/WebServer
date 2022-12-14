#include "http_conn.h"
#include <stdlib.h>
#include <sys/epoll.h>

// set new fd nonblock
void setnonblock(int fd){
	int old_flag = fcntl(fd, F_GETFL);
	int new_flag = old_flag | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flag);
	
}

// add fds needs to be listened to epoll
void addfd(int epollfd, int fd, bool one_shot){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLRDHUP; // listen 
	if (one_shot){ //prevent from multiple threads handling one socket
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); // add to the epoll fd

	// set fd nonblock
	setnonblock(fd);

}
//remove fds needs to be listened to 
void removefd(int epollfd, int fd){
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}
// modify fds needs to be listed to, reset the EPOLLONESHOT events on socket, make sure EPOLLIN will be triggered next time
void modfd(int epollfd, int fd, int ev){
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::init(int sockfd, const sock_in &addr) {
	m_sockfd = sockfd;
	m_address = addr;

	// set port reuse
	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	// add to the epoll entity
	addfd(m_epollfd, m_sockfd, true);
	m_user_count++; //total users add 1
}

void http_conn::close_conn(){
	if (m_sockfd != -1){
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--; // after closing one connection, total user num--;
	}
	
}

bool http_conn::read(){
	printf("read all data at once\n");
	return true;
}

bool http_conn::write(){
	printf("write all data at once\n");
	return true;
}

// envoked by working thread in the thread poll, which is the entrance function of HTTP requests
void http_conn::process(){
	// decode HTTP requests
	printf("pass request, creating response\n");
	
}
