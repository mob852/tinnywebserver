#include "server.h"
#include <cstdlib>

int main(int argc, char* argv[]) {
    int port = 8080;       // 默认端口
    int thread_num = 8;    // 默认线程数量

    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (argc >= 3) {
        thread_num = atoi(argv[2]);
    }

    Server server(port, thread_num);
    server.run();

    return 0;
}
