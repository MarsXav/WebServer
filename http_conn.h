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

    bool process_write(HTTP_CODE ret);

    bool add_response(const char* format, ...);
    bool add_status_line(int status,  const char* title);
    bool add_headers(int content_len);
    bool add_content_length(int content_len);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();
    bool add_content(const char *content);

private:
	int m_sockfd; // the HTTP socket connects;
	sockaddr_in m_address; // socket address used for communication

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_ind; //mark the index of the position of the client data

    int m_checked_index; // the current pointer to the character in the buffer
    int m_start_line; // the initial position of the parsing line

    char *m_url; //file name of the requested file
    char *m_version; //protocol version, which only supports HTTP1.1
    METHOD m_method;
    char *m_host;//host name
    bool m_linger; //identify if HTTP request keeps the connection

    char m_write_buf[WRITE_BUFFER_SIZE]; //write buffer
    int m_write_ind; // bytes of the data pending to be sent
    char *m_file_address; //the address of the mapped file
    struct stat m_file_state; //status of the target file
    struct iovec m_iv[2];
    int m_iv_count;

    int m_content_length;

    CHECK_STATE m_check_state; // the current status of the main state machine

    void init(); //initialise other connection data

    void unmap();

    char* get_line() {
        return m_read_buf + m_start_line;
    }

    HTTP_CODE do_request();



}

#endif
