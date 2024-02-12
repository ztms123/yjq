#include "http_conn.h"
#include <map>
#include <fstream>

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
const char *doc_root = "/mnt/hgfs/job/my_web/yjq/yjq/root";
const char *ok_200_title = "OK";
//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, m_sockfd, true);
    init();
}

void http_conn::init()
{
    bytes_have_send = 0;
    m_linger = false;
    m_start_line = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_checked_idx = 0;
    bytes_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
}

bool http_conn::read_once()
{
    if (m_read_idx > READ_BUFFER_SIZE)
    {
        return false;
    }
    
    int ret_byte = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    if (ret_byte <= 0)
    {
        return false;
    }

    m_read_idx += ret_byte;
    return true;
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    printf("parse_line \n");
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        if (m_read_buf[m_checked_idx] == '\r')
        {
            if (m_checked_idx + 1 == m_read_idx)
            {
                printf("LINE_OPEN \n");
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                printf("LINE_OK \n");
                m_read_buf[m_checked_idx] = '\0';
                m_read_buf[m_checked_idx + 1] = '\0';
                m_checked_idx += 2;
                return LINE_OK;
            }
            printf("LINE_BAD \n");
            return LINE_BAD;
        }
        printf("parse_line if \n");
    }
    return LINE_OPEN;
    printf("parse_line for \n");
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");  //去空格
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
  
    m_version += strspn(m_version, " \t");
    
    printf("m_version is %s \n", m_version);

    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    return GET_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    printf("do_request \n");
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
    {
        printf("stat(m_real_file, &m_file_stat) < 0");
        return NO_RESOURCE;
    }
        
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        printf("!(m_file_stat.st_mode & S_IROTH)");
        return FORBIDDEN_REQUEST;
    }
        
    if (S_ISDIR(m_file_stat.st_mode))
    {
        printf("S_ISDIR(m_file_stat.st_mode)");
        return BAD_REQUEST;
    }
        
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    printf("do_request end \n");
    return FILE_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    while (LINE_OK == parse_line())
    {
        char *text = get_line();
        m_start_line = m_checked_idx;
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            if (parse_request_line(text) == BAD_REQUEST) 
            {
                printf("parse_request_line ERR!!!");
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            if (parse_headers(text) == BAD_REQUEST) 
            {
                printf("parse_headers ERR!!!");
                return BAD_REQUEST;
            }
            else if (parse_headers(text) == GET_REQUEST) return do_request();
            break;
        case CHECK_STATE_CONTENT:
            parse_content(text);
            break;
        default:
            printf("m_check_state ERR!!!");
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case FILE_REQUEST:
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        return false;
        break;
    
    default:
        return false;
        break;
    }
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_to_send -= temp;

        // if (bytes_have_send >= m_iv[0].iov_len)
        // {
        //     m_iv[0].iov_len = 0;
        //     m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
        //     m_iv[1].iov_len = bytes_to_send;
        // }
        

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::process()
{
    printf("process \n");
    HTTP_CODE ret = process_read();
    if (ret == NO_REQUEST) 
    {
        printf("process_read ERR!!! \n");
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    if(!process_write(ret))
    {
        printf("process_write ERR!!! \n");
        close_conn();
        return;
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}