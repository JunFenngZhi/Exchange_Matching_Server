#include "server.h"
#include "socket.h"
#include "clientInfo.h"
#include "exception.h"
#include <pthread.h>
#include <string>
#include <cstdio>
#include <vector>

#define MAX_LENGTH 65536

using namespace std;

void server::run()
{
    connectDB("serverdb", "postgres", "passw0rd");

    // create server socket, listen to port
    int server_fd;
    try{
        server_fd = createServerSocket(portNum);
    }
    catch (const std::exception &e){
        std::cerr << e.what() << '\n';
        return;
    }

    // server keep runnning
    int client_id = 1;
    while (1)
    {
        // server accept new connection
        int client_fd;
        string clientIp;
        try{
            client_fd = serverAcceptConnection(server_fd, clientIp);
        }
        catch (const std::exception &e){
            std::cerr << e.what() << '\n';
            continue;
        }

        // accept new XML request
        vector<char> buffer(MAX_LENGTH, 0);
        int len = recv(client_fd,
                       &(buffer.data()[0]),
                       MAX_LENGTH,
                       0); // len 是recv实际的读取字节数
        if (len <= 0){
            std::cerr << "fail to accept request." << '\n';
            close(client_fd);
            continue;
        }
        
        // generate new thread for each request
        string XMLrequest(buffer.data(), len);
        clientInfo * info = new clientInfo(client_fd, client_id, XMLrequest);  //这里需要加锁吗？ 主线程delete会让其他线程崩溃
        pthread_t thread;
        pthread_create(&thread, NULL, handleRequest, info);

        client_id++;
    }
}

void server::connectDB(string dbName, string userName, string password)
{
    printf("connect to %s with user: %s, using password: %s\n", dbName.c_str(), userName.c_str(), password.c_str());
    return;
}

 void * server::handleRequest(void *info){
    //记得close clietn socket以及clean clientInfo
    clientInfo* client_info = (clientInfo*) info;
    client_info->showInfo();
    delete client_info;
 }