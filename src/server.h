#ifndef SERVER_H
#define SERVER_H

#include "ThreadPool.h"
#include <unordered_map>
#include <string>
#include "logger.h"

// 在server.h中
struct HttpRequest {
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

class Server {
public:
    Server(int port, int thread_num = 8); // 增加线程数量参数
    ~Server();

    void run();
    

private:
    void init_socket();
    void event_loop();
    void accept_connections();
    void handle_read(int fd);
    void handle_write(int fd);

    bool parse_request_header(const std::string& request_header, HttpRequest& request);
    bool parse_http_request(int fd) ;
    void handle_request(int fd, const HttpRequest& request); 
    void handle_get_request(int fd, const HttpRequest& request) ;
    void handle_post_request(int fd, const HttpRequest& request) ;
    void send_file_response(int fd, const std::string& file_path);
    std::string get_content_type(const std::string& file_path) ;
    void send_error_response(int fd, int status_code, const std::string& status_message) ;
    void init_logger();


    int port_;
    ThreadPool thread_pool_; // 线程池成员

    int listen_fd_;
    int epoll_fd_;

    // 添加一个映射，存储每个文件描述符对应的读缓冲区
    std::unordered_map<int, std::string> read_buffers_;
    // 添加一个映射，存储每个文件描述符对应的写缓冲区
    std::unordered_map<int, std::string> write_buffers_;
};

#endif // SERVER_H
