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
#include <stdlib.h>
#include "server.h"

#define QLEN          150
using namespace std;

bool Server::service(int sockfd){

    int tempStdinFd = dup(0);
    int tempStdoutFd = dup(1);
    int tempStderrFd = dup(2);
    dup2(sockfd, 0);
    dup2(sockfd, 1);
    dup2(sockfd, 2);

    int nowId = m_clientPool.findUser(sockfd);
    Mysh *cursh = m_clientPool.v_clients[nowId].mysh;
    cursh->isExit = false;

    //p.setENV("setenv PATH bin:. ");

    string tempStr;
    std::getline(std::cin,tempStr);
    size_t tempPos = tempStr.find_first_of("\r", 0);
    tempStr[tempPos] = '\n';

    /* 判断是否为Commuication 指令 */
    /* pos1 找非空格字符
     * pos2 找空格或换行字符
     */
    int pos1 = tempStr.find_first_not_of(' ', 0);
    int pos2 = tempStr.find_first_of(" \n", pos1);
    string cmd = tempStr.substr(pos1, pos2 - pos1);

    pos1 = tempStr.find_first_not_of(' ', pos2);

    pos2 = tempStr.find_first_of("\n", pos1);
    string msgline = tempStr.substr(pos1, pos2 - pos1);
    pos2 = tempStr.find_first_of(" \n", pos1);
    string nameline = tempStr.substr(pos1, pos2 - pos1);
    

    if ( cmd == "who") {
        m_clientPool.printUser(nowId); 
    } 
    else if (cmd == "name") {
        m_clientPool.changeName(nowId, nameline);
        string nameMsg = "*** User from ";
        nameMsg += m_clientPool.v_clients[nowId].ip;
        nameMsg += '/';
        nameMsg += m_clientPool.v_clients[nowId].port;
        nameMsg += " is named \'";
        nameMsg += nameline;
        nameMsg += "\'. ***\n";
        serverBroadcast(nameMsg);
    }
    else if (cmd == "yell") {
        string yellMsg = "*** The User ";
        yellMsg += m_clientPool.v_clients[nowId].name;
        yellMsg += " yealled ***: ";
        yellMsg += msgline + "\n";
        serverBroadcast(yellMsg);
    }
    else if (cmd == "tell") {
        int destId = atoi(nameline.c_str());
        string tellmsg = "*** ";
        tellmsg += m_clientPool.v_clients[nowId].name;
        tellmsg += " told you ***: ";
        /* 解析命令中的信息 */
        pos1 = tempStr.find_first_not_of(' ', pos2);
        pos2 = tempStr.find_first_of('\n', pos1);
        tellmsg += tempStr.substr(pos1, pos2 - pos1);
        tellmsg += '\n';
        sendMessage(destId - 1, nowId, tellmsg);
        //tell(tempStr);
    }
    else 
        cursh->parseCommand(tempStr);

    dup2(tempStdinFd, 0);
    dup2(tempStdoutFd, 1);
    dup2(tempStderrFd, 2);
    close(tempStdinFd);
    close(tempStdoutFd);
    close(tempStderrFd);

    if (cursh->isExit == true)
        return false;
    sendMessage(nowId, "% ");
    return true;
}

int Server::passivesock(string port) {
    struct protoent *ppe;
    struct sockaddr_in serv_addr;
    if ((ppe = getprotobyname("tcp")) == 0)
        error("can't get prtocal entry");

    /* Open a TCP socker (an Internet stream socket). */
    int msock = socket(PF_INET, SOCK_STREAM, ppe->p_proto);
    if (msock == -1)
        error("Can't open socket");

    /* Bind our local address so that the client can send to us. */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(port.c_str()));

    if (bind(msock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("Can't bind");
    printf("\t[Info] Binding...\n");

    if (listen(msock, QLEN) < 0)
        error("Can't listen");
    printf("\t[Info] Listening...\n");

    return msock;
}

Server::Server(string port) {
    int msock;
    struct sockaddr_in cli_addr;
    fd_set rfds;
    fd_set afds;
    socklen_t alen;
    int fd, nfds;

    msock = passivesock(port);
    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    while (true) {
        memcpy(&rfds, &afds, sizeof(rfds));

        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            error("Can't select");
        if (FD_ISSET(msock, &rfds)) {
            int ssock;
            alen = sizeof(cli_addr);

            ssock = accept(msock, (struct sockaddr *) &cli_addr, &alen);
            if (ssock < 0)
                error("Can't accept");

            string ip = inet_ntoa(cli_addr.sin_addr);
            string port = to_string((int)ntohs(cli_addr.sin_port));
            printf("\t[Info] Receive connection from %s\n", ip.c_str());
            int nowId = m_clientPool.addUser(ssock, ip, port);
            assert(nowId >= 0);

            string welcomeMsg = "****************************************\n";
            welcomeMsg       += "** Welcome to the information server. **\n";
            welcomeMsg       += "****************************************\n";

            sendMessage(nowId, welcomeMsg);
            string loginMsg = "*** User \'(no name)\' entered from ";
            loginMsg += ip;
            loginMsg += "/";
            loginMsg += port;
            loginMsg += ".***\n";

            serverBroadcast(loginMsg);
            sendMessage(nowId, "% ");

            FD_SET(ssock, &afds);
        }

        for (fd = 0; fd < nfds; ++fd) {
            if (fd != msock && FD_ISSET(fd, &rfds))
                if (service(fd) == false) {

                    int nowId = m_clientPool.findUser(fd);
                    /* 打印离线信息 */
                    string logoutMsg = "*** User \'";
                    logoutMsg += m_clientPool.v_clients[nowId].name;
                    logoutMsg += "\' left. ***\n";
                    serverBroadcast(logoutMsg);

                    /* 完成clentPool 的清理工作 */
                    m_clientPool.v_clients[nowId].isActive = false;
                    printf("\t[Info] Close connect from ip: %s\n", m_clientPool.v_clients[nowId].ip.c_str());
                    (void) close(fd);
                    FD_CLR(fd, &afds);
                }
        }

    }

} 

Server::~Server() {};

void Server::error(const char *eroMsg) {
    cerr << eroMsg << ":" << strerror(errno) << endl;
    exit(1);
}

/* client send to client */
void Server::sendMessage(int destID, int sourceID, string msg) {
    int destFD = m_clientPool.v_clients[destID].sock;
    int sourceFD = m_clientPool.v_clients[sourceID].sock;
    dup2(destFD, 1);
    /* 此处不加 flush 会出现未期的错误 */
    cout << msg << flush;
    dup2(sourceFD, 1);
}

/* 从线程广播消息 */
void Server::clientBroadcast(int sourceID, string msg) {
    for (string::size_type i = 0; i < m_clientPool.v_clients.size(); i++)
        if (m_clientPool.v_clients[i].isActive == true)
            sendMessage(i, sourceID, msg);
}

/* server send to client */
void Server::sendMessage(int destID, string msg) {
    int exchangFD = dup(1);
    int tarFD = m_clientPool.v_clients[destID].sock;
    //cout << "tarFD" << tarFD; //<< flush;
    dup2(tarFD, 1);
    cout << msg << flush;
    dup2(exchangFD, 1);
    close(exchangFD);
}

/* 主线程广播消息 */
void Server::serverBroadcast(string msg) {
    for (string::size_type i = 0; i < m_clientPool.v_clients.size(); i++)
        if (m_clientPool.v_clients[i].isActive == true)
            sendMessage(i, msg);
}

/* 处理通信指令 */

