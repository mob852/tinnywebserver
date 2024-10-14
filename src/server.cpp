#include "server.h"
#include "utils.h"
#include "epoll.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sstream>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>

Server::Server(int port, int thread_num)
    : port_(port), thread_pool_(thread_num) {
    init_socket();
    init_logger();     // 初始化日志系统
}
Server::~Server() {
    close(listen_fd_);
    close(epoll_fd_);
}

void Server::init_socket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    // 设置地址复用
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有地址
    server_addr.sin_port = htons(port_);

    if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error");
        close(listen_fd_);
        exit(EXIT_FAILURE);
    }

    // 开始监听
    if (listen(listen_fd_, SOMAXCONN) == -1) {
        perror("listen error");
        close(listen_fd_);
        exit(EXIT_FAILURE);
    }

    // 设置非阻塞
    set_nonblocking(listen_fd_);

    // 创建epoll实例
    epoll_fd_ = create_epoll_fd();

    // 将监听套接字加入epoll
    add_fd_to_epoll(epoll_fd_, listen_fd_, true); // 使用ET模式
}

void Server::run() {
    LOG_INFO("服务器启动，监听端口：" + std::to_string(port_));
    event_loop();  // 进入事件循环
    LOG_INFO("服务器停止运行。");
}

void Server::event_loop() {
    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (n == -1) {
            LOG_ERROR("epoll_wait() 错误：" + std::string(strerror(errno)));
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            // 错误事件处理
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                LOG_WARN("文件描述符 " + std::to_string(fd) + " 发生错误或挂起，关闭连接。");
                close(fd);
                continue;
            }

            if (fd == listen_fd_) {
                accept_connections();
            } else {
                // 将I/O事件的处理任务提交给线程池
                if (events[i].events & EPOLLIN) {
                    // 可读事件
                    thread_pool_.enqueue([this, fd]() { handle_read(fd); });
                } else if (events[i].events & EPOLLOUT) {
                    // 可写事件
                    thread_pool_.enqueue([this, fd]() { handle_write(fd); });
                }            
            }
            // if (fd == listen_fd_) {
            //     // 处理新的连接请求
            //     accept_connections();
            // } else if (events[i].events & EPOLLIN) {
            //     // 处理可读事件
            //     handle_read(fd);
            // } else if (events[i].events & EPOLLOUT) {
            //     // 处理可写事件
            //     handle_write(fd);
            // } else {
            //     continue;
            // }
        }
    }
}

// void Server::accept_connections() {
//     struct sockaddr_in client_addr;
//     socklen_t client_addrlen = sizeof(client_addr);

//     while (true) {
//         int conn_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_addrlen);
//         if (conn_fd == -1) {
//             if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                 // 所有连接都已处理完毕
//                 break;
//             } else {
//                 perror("accept error");
//                 break;
//             }
//         }

//         // 打印客户端信息
//         printf("Accepted connection from %s:%d\n",
//                inet_ntoa(client_addr.sin_addr),
//                ntohs(client_addr.sin_port));

//         // 设置新连接套接字为非阻塞模式
//         set_nonblocking(conn_fd);

//         // 将新连接套接字添加到epoll中
//         add_fd_to_epoll(epoll_fd_, conn_fd, true); // 使用ET模式
//     }
// }
void Server::accept_connections() {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int conn_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_addr_len);
    if (conn_fd == -1) {
        LOG_ERROR("accept() 错误：" + std::string(strerror(errno)));
        return;
    }

    // 设置非阻塞模式
    set_nonblocking(conn_fd);

    // 添加到 epoll 监听
    add_fd_to_epoll(epoll_fd_, conn_fd, EPOLLIN | EPOLLET);

    LOG_INFO("接受新连接，文件描述符：" + std::to_string(conn_fd) +
             ", 来自：" + inet_ntoa(client_addr.sin_addr) +
             ":" + std::to_string(ntohs(client_addr.sin_port)));
}


// 在server.cpp中
bool Server::parse_request_header(const std::string& request_header, HttpRequest& request) {
    std::istringstream stream(request_header);
    std::string line;

    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        if (!(line_stream >> request.method >> request.url >> request.version)) {
            return false;
        }
    } else {
        return false;
    }

    // 解析请求头部字段
    while (std::getline(stream, line) && line != "\r") {
        size_t pos = line.find(":");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // 去除可能的空格和回车
            key.erase(remove_if(key.begin(), key.end(), isspace), key.end());
            value.erase(remove_if(value.begin(), value.end(), isspace), value.end());
            request.headers[key] = value;
        }
    }
    return true;
}

bool Server::parse_http_request(int fd) {
    // 获取该连接的读缓冲区
    std::string& buffer = read_buffers_[fd];

    // 查找请求头和请求体的分隔位置（空行）
    size_t pos = buffer.find("\r\n\r\n");
    if (pos != std::string::npos) {
        // 已经接收到完整的请求头部

        // 提取请求头部
        std::string request_header = buffer.substr(0, pos + 4);

        // 解析请求行和请求头部
        HttpRequest request;
        if (parse_request_header(request_header, request)) {
            // 判断是否有请求体（针对POST请求）
            if (request.method == "POST") {
                // 获取Content-Length字段，确定请求体长度
                auto it = request.headers.find("Content-Length");
                if (it != request.headers.end()) {
                    int content_length = std::stoi(it->second);
                    // 检查是否接收到了完整的请求体
                    if (buffer.size() >= pos + 4 + content_length) {
                        // 提取请求体
                        request.body = buffer.substr(pos + 4, content_length);
                        // 处理请求
                        handle_request(fd, request);
                        // 清空缓冲区，为下一次请求做准备
                        buffer.erase(0, pos + 4 + content_length);
                        return true;
                    }
                } else {
                    // 没有Content-Length，无法确定请求体长度，返回错误
                    send_error_response(fd, 400, "Bad Request");
                    return false;
                }
            } else {
                // GET请求，无请求体
                // 处理请求
                handle_request(fd, request);
                // 清空缓冲区，为下一次请求做准备
                buffer.erase(0, pos + 4);
                return true;
            }
        } else {
            // 解析失败，返回错误响应
            send_error_response(fd, 400, "Bad Request");
            return false;
        }
    }
    // 未接收到完整的请求，继续等待
    return false;
}


void Server::handle_read(int fd) {
    LOG_DEBUG("处理读事件，文件描述符：" + std::to_string(fd));
    char buffer[4096];
    while (true) {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            // 将读取到的数据追加到读缓冲区中
            read_buffers_[fd].append(buffer, bytes_read);

            // 尝试解析HTTP请求
            if (parse_http_request(fd)) {
                // 解析成功，修改事件为EPOLLOUT，等待发送响应
                modify_fd_in_epoll(epoll_fd_, fd, EPOLLOUT | EPOLLET);
            }
            break;
        } else if (bytes_read == 0) {
            // 客户端关闭连接
            printf("Client disconnected on fd %d\n", fd);
            close(fd);
            read_buffers_.erase(fd); // 移除对应的缓冲区
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据已全部读取完毕
                break;
            } else {
                perror("read error");
                close(fd);
                read_buffers_.erase(fd); // 移除对应的缓冲区
                break;
            }
        }
    }
}

void Server::handle_request(int fd, const HttpRequest& request) {
    // 根据请求的方法和URL处理请求
    if (request.method == "GET") {
        handle_get_request(fd, request);
    } else if (request.method == "POST") {
        handle_post_request(fd, request);
    } else {
        // 不支持的方法，返回405错误
        send_error_response(fd, 405, "Method Not Allowed");
    }
}

void Server::handle_get_request(int fd, const HttpRequest& request) {
    // 根据请求的URL返回静态资源
    std::string file_path = "./resource" + request.url; // 假设网站根目录为当前目录

    // 如果请求的URL为"/"，则返回"/index.html"
    if (request.url == "/") {
        file_path = "./resource/index.html";
    }

    // 读取文件内容并发送响应
    send_file_response(fd, file_path);
}

void Server::handle_post_request(int fd, const HttpRequest& request) {
    // 处理POST请求，根据需求实现
    // 这里简单地返回请求体内容

    std::string response_body = "Received POST data:\n" + request.body;
    std::string response_header = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Content-Length: " + std::to_string(response_body.size()) + "\r\n"
                                  "\r\n";

    // 将响应头和响应体添加到待发送的缓冲区中
    write_buffers_[fd] = response_header + response_body;
}

void Server::send_file_response(int fd, const std::string& file_path) {
    // 打开文件
    int file_fd = open(file_path.c_str(), O_RDONLY);
    if (file_fd == -1) {
        // 文件不存在，返回404错误
        send_error_response(fd, 404, "Not Found");
        return;
    }

    // 获取文件大小
    struct stat stat_buf;
    fstat(file_fd, &stat_buf);
    size_t file_size = stat_buf.st_size;

    // 读取文件内容
    char* file_content = new char[file_size];
    read(file_fd, file_content, file_size);
    close(file_fd);

    // 确定Content-Type
    std::string content_type = get_content_type(file_path);

    // 构建响应头
    std::string response_header = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: " + content_type + "\r\n"
                                  "Content-Length: " + std::to_string(file_size) + "\r\n"
                                  "\r\n";

    // 将响应头和响应体添加到待发送的缓冲区中
    write_buffers_[fd] = response_header + std::string(file_content, file_size);

    delete[] file_content;
}

bool ends_with(const std::string& value, const std::string& ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string Server::get_content_type(const std::string& file_path) {
    if (ends_with(file_path, ".html") || ends_with(file_path, ".htm")) {
        return "text/html";
    } else if (ends_with(file_path, ".css")) {
        return "text/css";
    } else if (ends_with(file_path, ".js")) {
        return "application/javascript";
    } else if (ends_with(file_path, ".png")) {
        return "image/png";
    } else if (ends_with(file_path, ".jpg") || ends_with(file_path, ".jpeg")) {
        return "image/jpeg";
    } else if (ends_with(file_path, ".gif")) {
        return "image/gif";
    } else {
        return "application/octet-stream";
    }
}


void Server::send_error_response(int fd, int status_code, const std::string& status_message) {
    std::string response_body = "<html><body><h1>" + std::to_string(status_code) + " " + status_message + "</h1></body></html>";
    std::string response_header = "HTTP/1.1 " + std::to_string(status_code) + " " + status_message + "\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: " + std::to_string(response_body.size()) + "\r\n"
                                  "\r\n";
    // 将响应头和响应体添加到待发送的缓冲区中
    write_buffers_[fd] = response_header + response_body;

    // 修改事件为EPOLLOUT，等待发送响应
    modify_fd_in_epoll(epoll_fd_, fd, EPOLLOUT | EPOLLET);
}


void Server::handle_write(int fd) {
    LOG_DEBUG("处理写事件，文件描述符：" + std::to_string(fd));
    std::string& buffer = write_buffers_[fd];
    ssize_t bytes_written = write(fd, buffer.c_str(), buffer.size());
    if (bytes_written == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 写缓冲区已满，稍后再写
            return;
        } else {
            perror("write error");
            close(fd);
            write_buffers_.erase(fd);
            return;
        }
    }

    // 更新写缓冲区，移除已发送的数据
    buffer.erase(0, bytes_written);

    if (buffer.empty()) {
        // 数据已全部发送完毕，修改事件为EPOLLIN，继续监听读事件
        modify_fd_in_epoll(epoll_fd_, fd, EPOLLIN | EPOLLET);
        write_buffers_.erase(fd); // 移除写缓冲区
    }
}

// 在 server.cpp 中
void Server::init_logger() {
    Logger::get_instance().set_level(DEBUG);    // 设置日志级别（DEBUG、INFO、WARN、ERROR）
    Logger::get_instance().set_async(true);     // 设置为异步日志（true：异步，false：同步）
}

