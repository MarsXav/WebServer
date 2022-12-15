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

    static const int READ_BUFFER_SIZE = 2048; //read buffer size
    static const int WRITE_BUFFER_SIZE = 2048; // write buffer size

    // HTTP request methods
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};

    enum CHECKSTATE{CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    /*
     * Parsing client requests: main state machine state
     * CHECK_STATE_REQUESTLINE: analysing request line
     * CHECK_STATE_HEADER: analysing header
     * CHECK_STATE_CONTENT: analysing content
     */
    enum LINE_STATUS{LINE_OK = 0, LINE_BAD, LINE_OPEN};
    /*
     * LINE_OK: read a whole line
     * LINE_BAD: line error
     * LINE_OPEN: line not whole
     */
    enum HTTP_CODE{NO_REQUEST, GET_REQUEST, BAD REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};

	http_conn(){}
	~http_conn(){}

	void process(); // handle client's requests

	void init(int sockfd, const sockaddr_in &addr); // initialise new connected clients

	void close_conn(); //close connection

	bool read(); //nonblocking write
	bool write(); //nonblocking write

    HTTP_CODE process_read(); //parse HTTP requests
    HTTP_CODE parse_request_line(char *text); //parse HTTP request line
    HTTP_CODE parse_headers(char *text); //parse HTTP header
    HTTP_CODE parse_content(char *text);

    LINE_STATUS parse_line();


private:
	int m_sockfd; // the HTTP socket connects;
	sockaddr_in m_address; // socket adderess used for communication

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_ind; //mark the index of the position of the client data


}

#endif
