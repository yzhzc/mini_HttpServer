#include <iostream>
#include <string>
#include <memory>
#include "HttpServer.hpp"
#include "Usage.hpp"



int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        Usage(argv[0]);
        exit(4);
    }
    int port = atoi(argv[1]);

    // 创建并启动Http服务执行业务
    std::shared_ptr<HttpServer> http_server(new HttpServer(port));
    http_server->InitServer();
    http_server->Loop();


    return 0;
}