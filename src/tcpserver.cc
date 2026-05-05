#include "tcpserver.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unordered_map>
#include "ThreadPool.h"
#include "log.h"
#include "lst_timer.h"

const int MAX_EVENTS = 10000;
const int MAX_FD = 65536; 
const int TIMESLOT = 5; 

static int pipefd[2];           
static time_heap timer_lst(10000);
static int epoll_fd = 0;
static std::unordered_map<int, TcpConnectionPtr> m_connections;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
    m_connections.erase(fd); 
}

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void cb_func(client_data* user_data) {
    if (!user_data) return;
    removefd(epoll_fd, user_data->sockfd);
    LOG_INFO("Kick Client (Timeout): fd=%d", user_data->sockfd);
}

TcpServer::TcpServer(const std::string &ip, uint16_t port) 
    : m_ip(ip), m_port(port) 
{
}

void TcpServer::SetConnectionCallback(const ConnectionCallback &cb) { 
    m_connectionCallback = cb; 
}

void TcpServer::SetMessageCallback(const MessageCallback &cb) { 
    m_messageCallback = cb; 
}

void TcpServer::Start() {
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, m_ip.c_str(), &server_addr.sin_addr);
    server_addr.sin_port = htons(m_port);
    
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, MAX_EVENTS);

    epoll_fd = epoll_create1(0);
    addfd(epoll_fd, server_fd, false);
    
    socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    setnonblocking(pipefd[1]); 
    addfd(epoll_fd, pipefd[0], false); 

    addsig(SIGALRM); 
    addsig(SIGTERM); 
    addsig(SIGINT);  
    alarm(TIMESLOT); 

    ThreadPool pool(4);
    client_data *users_timer = new client_data[MAX_FD];
    struct epoll_event events[MAX_EVENTS];
    bool timeout = false;
    bool stop_server = false;

    LOG_INFO("RPC Network Reactor Start on %s:%d...", m_ip.c_str(), m_port);

    while (!stop_server) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        if (n < 0 && errno != EINTR) {
            LOG_ERROR("Epoll Failure");
            break;
        }

        for (int i = 0; i < n; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                while (true) {
                    int connfd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                    if (connfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        break;
                    }
                    
                    TcpConnectionPtr conn(new TcpConnection(connfd));
                    // 向下层连接对象注入回调
                    conn->SetConnectionCallback(m_connectionCallback);
                    conn->SetMessageCallback(m_messageCallback);
                    
                    m_connections[connfd] = conn;
                    addfd(epoll_fd, connfd, true);

                    users_timer[connfd].address = client_addr;
                    users_timer[connfd].sockfd = connfd;
                    
                    util_timer *timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    timer->expire = time(NULL) + 3 * TIMESLOT; 
                    
                    users_timer[connfd].timer = timer;
                    timer_lst.add_timer(timer);

                    // 触发新连接回调
                    if (m_connectionCallback) {
                        m_connectionCallback(conn);
                    }
                }
            }
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                char signals[1024];
                int ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0) continue;
                else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[j]) {
                            case SIGALRM: timeout = true; break;
                            case SIGTERM:
                            case SIGINT: stop_server = true; break;
                        }
                    }
                }
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                util_timer *timer = users_timer[sockfd].timer;
                if (timer) timer_lst.del_timer(timer);
                removefd(epoll_fd, sockfd); 
            }
            else if (events[i].events & EPOLLIN) {
                util_timer *timer = users_timer[sockfd].timer;
                if (timer) timer_lst.adjust_timer(timer); 
                
                auto it = m_connections.find(sockfd);
                if (it != m_connections.end()) {
                    it->second->HandleRead();
                    if (!it->second->connected()) {
                        removefd(epoll_fd, sockfd);
                    } else {
                        epoll_event event;
                        event.data.fd = sockfd;
                        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT) {
                util_timer *timer = users_timer[sockfd].timer;
                if (timer) timer_lst.adjust_timer(timer);
                
                auto it = m_connections.find(sockfd);
                if (it != m_connections.end()) {
                    it->second->HandleWrite();
                    if (!it->second->connected()) {
                        removefd(epoll_fd, sockfd);
                    } else {
                        epoll_event event;
                        event.data.fd = sockfd;
                        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
                    }
                }
            }
        }

        if (timeout) {
            timer_lst.tick();
            alarm(TIMESLOT);
            timeout = false;
        }
    }
    
    close(epoll_fd);
    close(server_fd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users_timer;
}