#include <cassert>
#include <errno.h>
#include <netdb.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>

#include "clientPool.h"
#include "fifo.h"


class Server {
    public:
        Server();
        Server(string port);
        ~Server();
        int passivesock(string port);
        bool service(int sockfd);

        void sendMessage(int destID, int sourceID, string msg);
        void sendMessage(int destID, string msg);
        void clientBroadcast(int sourceID, string msg);
        void serverBroadcast(string msg);
        void error(const char* eroMsg);
        bool isNameExist(string s);

        /* 信号处理封装函数，注册处理函数 */
        static int catchSignal(int sig, void (* handle)(int));
        static void sigintHandle(int sig);
        static void sigchldHandle(int sig);
        static void sigusrHandle(int sig);

        /* 通讯命令处理函数
         * 由client 发送信号唤醒server 调用实现通信功能*/
        void whoHandle(int fd);
        void nameHandle(int fd, string s);
        void tellHandle(int fd, string s);
        void yellHandle(int fd, string s);

        /* client 部分对通信命令的响应 */
        void whoClient(int nowId);
        void nameClient(int nowId);
        void tellClient(int nowId);
        void yellClient(int nowId);

        void copyToShmemory(string cmd, char *message);

        /* parent 进程处理public pipe */ 
        bool preFifoParse(string cmdline, int nowFd);
        void pipeCharEarse(string &cmdline);

        /* 当前进程处理public pipe */
        bool preFifoClient(string &cmdline, int nowId, int &readFD, int &write);

        
    public:
        ClientPool m_clientPool;
        Fifo m_fifo;
        

};


typedef struct {
    bool isSuccess;
    int srcSocketFD;
    int srcPid;
    char message[3][1024];
} Mesg;
