#include "http_conn.h"
#include <stdlib.h>
#include <sys/epoll.h>

const char* doc_root = "/home/mars/WebServer/resources";
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible";
const char* error_403_title = "Forbidden";
const char* error_403_form = "The resources are unusable";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file wes not found on this server";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file";
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

// set new fd nonblock
void setnonblock(int fd){
	int old_flag = fcntl(fd, F_GETFL);
	int new_flag = old_flag | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flag);
}

// add fds needs to be listened to epoll
void addfd(int epollfd, int fd, bool one_shot){
	epoll_event event{};
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLRDHUP; // listen 
	if (one_shot){ //prevent from multiple threads handling one socket
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); // add to the epoll fd
	// set fd nonblock to avoid blocking when no data is received
	setnonblock(fd);
}
//remove fds needs to be listened to 
void removefd(int epollfd, int fd){
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
	close(fd);
}
// modify fds needs to be listed to, reset the EPOLLONESHOT events on socket, make sure EPOLLIN will be triggered next time
void modfd(int epollfd, int fd, int ev){
	epoll_event event{};
	event.data.fd = fd;
	event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::init(int sockfd, const sockaddr_in &addr) {//initialise new accepted connections
	m_sockfd = sockfd; // for later handling
	m_address = addr;
	// set port reuse
	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	// add to the epoll entity
	addfd(m_epollfd, m_sockfd, true);
	m_user_count++; //total users add 1
    init();
}

void http_conn::init(){
    m_check_state = CHECK_STATE_REQUESTLINE; //initial state is set as parsing first request line
    m_checked_index = 0;
    m_start_line = 0;
    m_read_ind = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_linger = false;
    m_host = 0;
    m_content_length = 0;
    bzero(m_read_buf, READ_BUFFER_SIZE); //clear the read buffer
}

void http_conn::close_conn(){
	if (m_sockfd != -1){
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--; // after closing one connection, total user num--;
	}
}

bool http_conn::read(){ //nonblock read, read data nonstop until no data remains
    if (m_read_ind >= READ_BUFFER_SIZE) {
        return false;
    }
    // bytes read
    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf+m_read_ind, READ_BUFFER_SIZE-m_read_ind, 0);
        if (bytes_read == -1){
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                //no data returned
                break;
            }
            return false;
        } else if (bytes_read == 0){
            // connection closed
            return false;
        }
        m_read_ind += bytes_read;
    }
    printf("data received: %s\n", m_read_buf);
	return true;
}

// main state machine
http_conn::HTTP_CODE http_conn::process_read() {//parse HTTP requests
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = {};
    while((m_check_state == CHECK_STATE_CONTENT)  // the main state machine is parsing the request content
            and (line_status == LINE_OK) // the child state machine is parsing the line
            or (line_status = parse_line()) == LINE_OK){
        // parsing a full line of data or parsing a request body
        text = get_line();
        m_start_line = m_checked_index;
        printf("got 1 http line: %s\n", text);
        // Finite state machine
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (ret == GET_REQUEST){
                    return do_request(); //parsing content
                }
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if (ret == GET_REQUEST){
                    return do_request();
                } else {
                    line_status = LINE_OPEN;
                }
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text) { //parse HTTP request line, obtain request method, URL and HTTP version
    // format: text = GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t"); //search a pointer which points to the first occurrence of the string the second param points to
    *m_url++ = '\0'; // text = GET\0/index.html HTTP/1.1
    char *method = text;
    if (strcasecmp(method, "GET") == 0) { //compares two strings
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }
    // m_url = index.html HTTP/1.1
    m_version = strpbrk(m_url, " \t"); //m_version = HTTP/1.1
    if (!m_version){
        return BAD_REQUEST;
    }
    // m_url = /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") == 0) {
        return BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/'); //find the first occurrence of a character
    }
    if (!m_url or m_url[0] != '/') {
        return BAD_REQUEST;
    }
    m_check_state = CHECK_STATE_HEADER; // check state changed to CHECK_STATE_HEADER
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text) {//parse HTTP header
    if (text[0] == '\0') { //if empty line detected, meaning that the header is finished parsing
        if (m_content_length != 0){
            // if HTTP request has information body, m_content_length bytes of information needs to be read
            // state of the state machine converts to CHECK_STATE_CONTENT
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //else we have obtained a complete HTTP request
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0){
        // paring Connection header section, Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        } else if (strncasecmp(text, "Content-Length:", 15) == 0){
            // parsing the Content-Length header section
            text += 15;
            text += strspn(text, " \t");
            m_content_length = atol(text);
        } else if (strncasecmp(text, "Host:", 5) == 0){
            //parsing the Host header section
            text += 5;
            text += strspn(text, " \t");
            m_host = text;
        } else {
            printf("unknown header: %s\n", text);
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text){
    // we do not parse information body, only tell if it has been read thoroughly
    if (m_read_ind >= (m_content_length + m_checked_index)){
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//child state machine
http_conn::LINE_STATUS http_conn::parse_line(){ //parsing by detecting \r,\n
    char temp;
    for (; m_checked_index < m_read_ind; ++m_checked_index){
        temp = m_read_buf[m_checked_index];
        if (temp == '\r') {
            if ((m_checked_index+1) == m_read_ind) { // no following char after '\r'
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_index+1] == '\n') {
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n'){
            if ((m_checked_index > 1) and (m_read_buf[m_checked_index-1] == '\r')){
                m_read_buf[m_checked_index-1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OK;
}

http_conn::HTTP_CODE http_conn::do_request(){
    // when we obtain a complete and correct HTTP request, we analyse the target file attributes
    // if the target file exists and readable for all users and not the directory, use mmap to map the memory address to m_file_address
    // a network resource will be accessed
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    // obtain m_real_file state information, -1 on fail, 0 on success
    if (stat(m_real_file, &m_file_stat) < 0){
        return NO_RESOURCE;
    }
    // identify access permission
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode)){ //identify if is a directory
        return BAD_REQUEST;
    }
    int fd = open(m_real_file, O_RDONLY);
    //create a mmap
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd);
    close(fd);
    return FILE_REQUEST;
}

//unmap the memory map
void http_conn::unmap() {
    if (m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::add_response(const char *format, ...) { //write data to the write buffer
    if (m_write_ind >= WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_ind, WRITE_BUFFER_SIZE - 1 - m_write_ind, format, arg_list);
    if (len >= WRITE_BUFFER_SIZE - 1 - m_write_ind) return false;
    m_write_ind += len;
    va_end(arg_list);
    return true;
}
bool http_conn::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}
bool http_conn::add_content_length(int content_len) {
    return add_response("Content_Length: %d\r\n", content_len);
}
bool http_conn::add_content_type(){
    return add_reponse("Content-Type: %s\r\n", "text/html");
}
bool http_conn::add_linger() {
    return add_response("Connection: %s\r\n", m_linger == true ? "keepalive" : "close");
}
bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool http_conn::write(){ //nonblock write
    int temp = 0;
    int bytes_have_sent = 0;
    int bytes_to_send = m_write_ind;

    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(1){
        temp = writev(m_sockfd, m_iv, m_iv_count); // writev writes data discretely
        if (temp <= -1) {
            if (errono == EAGAIN){
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_sent += temp;
        if (bytes_to_send <= bytes_have_sent){
            unmap();
            if (m_linger){
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            } else {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }


	return true;
}

bool http_conn::process_write(HTTP_CODE ret) {
    switch(ret){
        case INTERNAL_ERROR:
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)){
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form)){
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form)){
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)){
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title);
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_ind;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        default:
            return false;
    }
}

// evoked by working thread in the thread poll, which is the entrance function of HTTP requests
void http_conn::process(){
	// decode HTTP requests
    HTTP_CODE read_ret = process_read();
    if (read_ret  == NO_REQUEST) { //request incomplete, needs to continue reading client's data
        modfd(m_epollfd, m_sockfd, EPOLLIN);
    }
    // generate response
    printf("parse request, creating response\n");
    bool write_ret = process_write(read_ret);
    if (!write_ret) close_conn();
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}
