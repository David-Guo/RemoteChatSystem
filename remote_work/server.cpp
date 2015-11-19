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
#include <fcntl.h>
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
    cursh->PATH = m_clientPool.v_clients[nowId].PATH;
    setenv("PATH", cursh->PATH.c_str(), 2);
    //cursh->setEnv(cursh->PATH);
    cursh->isExit = false;

    //p.setENV("setenv PATH bin:. ");

    string tempStr;
    std::getline(std::cin,tempStr);
    size_t tempPos = tempStr.find_first_of("\r", 0);
    if ( tempPos != string::npos )
        tempStr[tempPos] = '\n';

    /* 判断是否为Commuication 指令 */
    /* pos1 找非空格字符
     * pos2 找空格或换行字符
     */
    size_t pos1 = tempStr.find_first_not_of(' ', 0);
    int pos2 = tempStr.find_first_of(" \n", pos1);
    string cmd = tempStr.substr(pos1, pos2 - pos1);

    pos1 = tempStr.find_first_not_of(' ', pos2);
    string nameline, msgline;
    if ( pos1 != string::npos ) {

        pos2 = tempStr.find_first_of("\n", pos1);
        string msgline = tempStr.substr(pos1, pos2 - pos1);
        pos2 = tempStr.find_first_of(" \n", pos1);
        string nameline = tempStr.substr(pos1, pos2 - pos1);
    }

    /* 判断public pipe 是否存在与存储错误信息 */
    int readFD = -1;
    int writeFD = -1;
    string readMsg = "";
    string writeMsg = "";

    if ( cmd == "who") {
        m_clientPool.printUser(nowId); 
    } 
    else if (cmd == "name") {
        if (isNameExist(nameline) == false) {
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
        else {
            string nameMsg = "*** User \'";
            nameMsg += nameline;
            nameMsg += "\' already exists. ***\n";
            sendMessage(nowId, nameMsg);
        }
    }
    else if (cmd == "yell") {
        string yellMsg = "*** The User ";
        yellMsg += m_clientPool.v_clients[nowId].name;
        yellMsg += " yealed ***: ";
        yellMsg += msgline + "\n";
        serverBroadcast(yellMsg);
    }
    else if (cmd == "tell") {
        int destId = atoi(nameline.c_str());
        /* 错误处理给自己发错误信息 */
        if (string::size_type(destId) > m_clientPool.v_clients.size() || m_clientPool.v_clients[destId - 1].isActive == false) {
            string tellmsg = "*** Error: User #";
            tellmsg += nameline;
            tellmsg += " does not exist yet. ***\n";
            sendMessage(nowId, tellmsg);
        }
        else { 
            string tellmsg = "*** ";
            tellmsg += m_clientPool.v_clients[nowId].name;
            tellmsg += " told you ***: ";
            /* 解析命令中的信息 */
            pos1 = tempStr.find_first_not_of(' ', pos2);
            pos2 = tempStr.find_first_of('\n', pos1);
            tellmsg += tempStr.substr(pos1, pos2 - pos1);
            tellmsg += '\n';
            sendMessage(destId - 1, nowId, tellmsg);
        }
    }
    /* 不是内建通信命令 */
    else {
        if (preFifoParse(tempStr, nowId, readFD, writeFD) == true) {
            pipeCharEarse(tempStr);
            cursh->parseCommand(tempStr);
            m_clientPool.v_clients[nowId].PATH = cursh->PATH;
        }
    }

    if (readFD != -1) 
        close(readFD);
    if (writeFD != -1)
        close(writeFD);

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

bool Server::preFifoParse(string cmdline, int nowId, int &readFD, int &writeFD) {

    /* 字符 <> 出现的Postion */
    size_t ioPos = 0;
    string readMsg;
    string writeMsg;
    string readError;
    string writeError;

    while (1) {
        ioPos = cmdline.find_first_of("<>", ioPos + 1);
        if (ioPos == string::npos)
            break;

        if (cmdline[ioPos + 1] != ' ') {
            int numPos = cmdline.find_first_of(" \n", ioPos + 1);
            int fifoId = atoi(cmdline.substr(ioPos + 1, numPos - ioPos + 1).c_str());

            if (cmdline[ioPos] == '<') {
                /* writestate == 1表示文件存在，广播读取信息，重置wirteState */
                if (m_fifo.v_writeState[fifoId] == 1) {
                    readMsg = "*** ";
                    readMsg += m_clientPool.v_clients[nowId].name;
                    readMsg += " (#";
                    readMsg += to_string(nowId + 1);
                    readMsg += ") just received via \'";
                    string tempStr = cmdline;
                    tempStr.pop_back();
                    readMsg += tempStr;
                    readMsg += "\'\n";
                    serverBroadcast(readMsg);

                    string filename = m_fifo.v_fileName[fifoId];
                    int tempFD = open(filename.c_str(), O_RDONLY, 0666);
                    assert(tempFD > 0);
                    readFD = tempFD;
                    dup2(tempFD, 0);
                    m_fifo.v_writeState[fifoId] = 0;

                }
                /* writestate == 0 表示文件不存在数据, 打印错误信息*/
                else {
                    readError = "*** Error: public pipe ";
                    readError += to_string(fifoId);
                    readError += " does not exist yet.***\n";
                    sendMessage(nowId, readError);
                    return false;
                }
            }
            else {
                if (m_fifo.v_writeState[fifoId] == 0) {
                    writeMsg = "*** ";
                    writeMsg += m_clientPool.v_clients[nowId].name;
                    writeMsg += " (#";
                    writeMsg += to_string(nowId + 1);
                    writeMsg += ") just piped \'";
                    string tempStr = cmdline;
                    tempStr.pop_back();
                    writeMsg += tempStr;
                    writeMsg += "\'\n";
                    serverBroadcast(writeMsg);

                    string filename = m_fifo.v_fileName[fifoId];
                    int tempFD = open(filename.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);
                    assert(tempFD > 0);
                    writeFD = tempFD;
                    dup2(tempFD, 1);
                    m_fifo.v_writeState[fifoId] = 1;
                }
                else {
                    writeError = "*** Error: public pipe ";
                    writeError += to_string(fifoId);
                    writeError += " already exists. ***\n";
                    sendMessage(nowId, writeError);
                    return false;
                }
            }
        }
        /* 如果字符<> 的后一个字符是空格 */
        else 
            continue;
    }

    return true;
}


void Server::pipeCharEarse(string &cmdline) {
    size_t pos = 0;
    string tempStr;

    bool isBreakChar = false;
    for (pos = 0; pos < cmdline.size(); pos++) {
        if(!isBreakChar && cmdline[pos] != '<' && cmdline[pos] != '>')
            tempStr += cmdline[pos];
        else if ((cmdline[pos] == '<' || cmdline[pos] == '>') && cmdline[pos + 1] != ' ' && cmdline[pos + 1] != '\n') 
            isBreakChar = true;
        else if (cmdline[pos] == '<' || cmdline[pos] == '>')
            tempStr += cmdline[pos];
        else if (cmdline[pos] == ' ' || cmdline[pos] == '\n') {
            tempStr += cmdline[pos];
            isBreakChar = false;
        }
    }

    cmdline = tempStr;
}


bool Server::isNameExist(string s) {
    for (string::size_type i = 0; i < m_clientPool.v_clients.size(); i++) {
        if (s == m_clientPool.v_clients[i].name)
            return true;
    }
    return false;
}




