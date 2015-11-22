#include "server.h"

using namespace std;

#define QLEN 30

#define shmKey ((key_t)  789)

Mesg *pMesg;
Server *thisServer;
int shmid;
int msock;


bool Server::service(int sockfd){

    int tempStdinFd = dup(0);
    int tempStdoutFd = dup(1);
    int tempStderrFd = dup(2);
    dup2(sockfd, 0);
    dup2(sockfd, 1);
    dup2(sockfd, 2);

    int nowId = m_clientPool.findUser(sockfd);
    Mysh *cursh = m_clientPool.v_clients[nowId].mysh;
    //cursh->PATH = m_clientPool.v_clients[nowId].PATH;
    //setenv("PATH", cursh->PATH.c_str(), 2);
    //cursh->setEnv(cursh->PATH);
    //cursh->isExit = false;

    //p.setENV("setenv PATH bin:. ");
    while (1) {
        string tempStr;
        std::getline(std::cin,tempStr);
        size_t tempPos = tempStr.find_first_of("\r", 0);
        if (tempPos != string::npos)
            tempStr[tempPos] = '\n';
        if (tempStr.find_first_of("\n", 0) == string::npos)
            tempStr += '\n';

        /* 判断是否为Commuication 指令 */
        /* pos1 找非空格字符
         * pos2 找空格或换行字符
         */
        size_t pos1 = tempStr.find_first_not_of(' ', 0);
        int pos2 = tempStr.find_first_of(" \n", pos1);
        string cmd = tempStr.substr(pos1, pos2 - pos1);

        pos1 = tempStr.find_first_not_of(' ', pos2);
        string nameline = "";
        string msgline  = "";
        if ( pos1 != string::npos ) {

            pos2 = tempStr.find_first_of("\n", pos1);
            /* yell 和 tell 使用msgline */
            msgline = tempStr.substr(pos1, pos2 - pos1);
            pos2 = tempStr.find_first_of(" \n", pos1);
            /* name 使用 nameline */
            nameline = tempStr.substr(pos1, pos2 - pos1);
        }

        /* 判断public pipe 是否存在与存储错误信息 */
        int readFD = -1;
        int writeFD = -1;
        string readMsg = "";
        string writeMsg = "";
        /* 通信命令处理 */
        if (cmd == "who") {
            /* attach share memory */
            if ((pMesg = (Mesg*)shmat(shmid, NULL, 0)) ==  (Mesg*)-1) 
                error("shmat failed");
            /* write to share memory */
            pMesg->srcSocketFD = sockfd;
            copyToShmemory("who", pMesg->message[0]);
            pMesg->srcPid = getpid();
            /* detach */
            if (shmdt(pMesg) < 0) cout << "shmdt failed" << endl;
            /* 发送信号唤醒parent */
            pid_t ppid = getppid();
            kill(ppid, SIGUSR1);
            pause();
        } 
        else if (cmd == "name") {
            assert(nameline != "");
            /* attach share memory */
            if ((pMesg = (Mesg*)shmat(shmid, NULL, 0)) ==  (Mesg*)-1) 
                error("shmat failed");
            /* write to share memory */
            pMesg->srcSocketFD = sockfd;
            copyToShmemory("name", pMesg->message[0]);
            copyToShmemory(nameline, pMesg->message[1]);
            pMesg->srcPid = getpid();

            if (shmdt(pMesg) < 0) cout << "shmdt failed" << endl;

            /* 发送信号唤醒parent */
            pid_t ppid = getppid();
            kill(ppid, SIGUSR1);
            pause();
        }
        else if (cmd == "yell" || cmd ==  "tell") {
            assert(nameline != "");
            /* attach share memory */
            if ((pMesg = (Mesg*)shmat(shmid, NULL, 0)) ==  (Mesg*)-1) 
                error("shmat failed");
            /* write to share memory */
            pMesg->srcSocketFD = sockfd;
            copyToShmemory(cmd, pMesg->message[0]);
            copyToShmemory(msgline, pMesg->message[1]);
            pMesg->srcPid = getpid();
            /* detach */
            if (shmdt(pMesg) < 0) cout << "shmdt failed" << endl;
            /* 发送信号唤醒parent */
            pid_t ppid = getppid();
            kill(ppid, SIGUSR1);
            pause();
        }
        /* 不是内建通信命令 */
        else {
            if (preFifoClient(tempStr, nowId, readFD, writeFD) == true) {
                pipeCharEarse(tempStr);
                cursh->parseCommand(tempStr);
                m_clientPool.v_clients[nowId].PATH = cursh->PATH;
            }
        }

        if (readFD != -1) 
            close(readFD);
        if (writeFD != -1)
            close(writeFD);

        dup2(sockfd, 0);
        dup2(sockfd, 1);

        if (cursh->isExit == true)
        {   
            /*dup2(tempStderrFd ,2);*/
            //dup2(tempStdinFd, 0);
            /*dup2(tempStdoutFd, 1);*/
            return false;
        }
        cout << "% "; 

    }
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
    /* 初始化thisServer */
    thisServer = this;

    /* 创建share memory segment，父子进程都拥有 */
    if ((shmid = shmget(shmKey, sizeof(Mesg), IPC_CREAT | 0666)) < 0) {
        cerr << "Creat share memory failed." << endl;
        exit(1);
    }

    /* Parent signal handle */
    if (catchSignal(SIGINT, Server::sigintHandle) == -1)
        cerr << "Handle SIGINT failed." << endl;
    if (catchSignal(SIGCHLD, SIG_IGN) == -1) 
        cerr << "Handle SIGCHLD failed." << endl;
    /* 向父进程发送信号，处理通信命令 */
    if (catchSignal(SIGUSR1, Server::sigusrHandle) == -1)
        cerr << "Handle SIGUSR1 failed." << endl;
    /* 唤醒重新唤醒子进程，不然子进程block */
    if (catchSignal(SIGUSR2, Server::sigusrHandle) == -1)
        cerr << "Handle ISGUSR2 failed." << endl;

    /* Bind and Listening to get the master sockfd */
    msock = passivesock(port);
    struct sockaddr_in cli_addr;
    socklen_t alen = sizeof(cli_addr);

    while (1) {
        int newsockfd = accept(msock, (struct sockaddr *) &cli_addr, &alen);
        if (newsockfd < 0) {
            close(newsockfd);
            continue;
        }

        string ip = inet_ntoa(cli_addr.sin_addr);
        string port = to_string((int)ntohs(cli_addr.sin_port));
        printf("\t[Info] Receive connection from %s\n", ip.c_str());

        int nowId = m_clientPool.addUser(newsockfd, "CGILAD", "511");
        assert(nowId >= 0);

        string welcomeMsg = "****************************************\n";
        welcomeMsg       += "** Welcome to the information server. **\n";
        welcomeMsg       += "****************************************\n";

        sendMessage(nowId, welcomeMsg);
        string loginMsg = "*** User \'(no name)\' entered from ";
        loginMsg += "CGILAG";
        loginMsg += "/";
        loginMsg += "511";
        loginMsg += ". ***\n";

        serverBroadcast(loginMsg);
        sendMessage(nowId, "% ");

        int childpid;
        if ((childpid = fork()) < 0) 
            cerr << "server: fork error" << endl;
        else if (childpid == 0) { 
            if(msock) close(msock);
            // child process
            signal(SIGCHLD, NULL);
            /* 关闭其他进程的socket */
            m_clientPool.closeOtherSocket(nowId);
            /* 处理请求业务 */
            service(newsockfd);
            /* client 退出 */
            /* attach the segment to our data space */
            if ((pMesg = (Mesg*) shmat(shmid, NULL, 0)) == (Mesg *) - 1) {
                cerr << "shamt failed " << endl;
                exit(1);
            }
            pMesg->srcSocketFD = newsockfd;
            copyToShmemory("exit", pMesg->message[0]);
            pMesg->srcPid = getpid();

            if (shmdt(pMesg) < 0) 
                error("shmdt failed");
            /* 唤醒parent */    
            pid_t ppid = getppid();
            kill(ppid, SIGUSR1);
            pause();
            close(msock);
            exit(0);
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


bool Server::preFifoParse(string cmdline, int nowFd) {
    int nowId = m_clientPool.findUser(nowFd);
    /* 没有下面的dup切换，非通讯命令会block 目前还不知道为什么 */
    int tempStdoutFd = dup(1);
    int tempStderrFd = dup(2);
    dup2(nowFd, 1);
    dup2(nowFd, 2);

    /* 字符 <> 出现的Postion */
    int readFD = -1, writeFD = -1;
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
            int fifoId = atoi(cmdline.substr(ioPos + 1, numPos - ioPos - 1).c_str());

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

                    readFD = fifoId;
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

                    writeFD = fifoId;
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

    dup2(tempStdoutFd, 1);
    dup2(tempStderrFd, 2);
    /* 传递fifo 文件打开信息到share memory 0 1 */
    copyToShmemory(to_string(readFD), pMesg->message[0]);
    copyToShmemory(to_string(writeFD), pMesg->message[1]);
    return true;
}


bool Server::preFifoClient(string cmdline, int nowId, int &readFD, int &writeFD) {

    if ((pMesg = (Mesg *) shmat(shmid, NULL, 0)) == (Mesg*) - 1)
        error("shamt failed");
    /* 唤醒parent 进程解析public pipe */
    pMesg->srcSocketFD = m_clientPool.v_clients[nowId].sock;
    copyToShmemory("publicPipe", pMesg->message[0]);
    copyToShmemory(cmdline, pMesg->message[1]);
    pMesg->srcPid = getpid();

    if (shmdt(pMesg) < 0) 
        error("shmdt failed");

    pid_t ppid = getppid();
    kill(ppid, SIGUSR1);
    pause();
    /* 从share memory 中取出解析结果 */
    if ((pMesg = (Mesg *)shmat(shmid, NULL, 0)) == (Mesg *) -1) 
        error("shmat failed");

    string message[2];
    message[0] = pMesg->message[0];
    message[1] = pMesg->message[1];

    if (shmdt(pMesg) < 0)
        error("shmdt, failed");

    if (message[0] != "-1") {

        string filename = m_fifo.v_fileName[atoi(message[0].c_str())];
        int tempFD = open(filename.c_str(), O_RDONLY, 0666);
        assert(tempFD > 0);
        readFD = tempFD;
        dup2(readFD, 0);
    }
    if (message[1] != "-1") {

        string filename = m_fifo.v_fileName[atoi(message[1].c_str())];
        //cout << filename << endl;
        int tempFD = open(filename.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);
        assert(tempFD > 0);
        writeFD = tempFD;
        dup2(writeFD, 1);
    }
    return true;
}


void Server::pipeCharEarse(string &cmdline) { 
    size_t pos = 0; string tempStr; 
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


int Server::catchSignal(int sig, void (*handle)(int)) {
    struct sigaction action;
    action.sa_handler = handle;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(sig, &action, NULL);
}


void Server::sigchldHandle(int sig) {
    int status;
    while(wait3(&status, WNOHANG, NULL))
        ;
}


void Server::sigintHandle(int sig) {
    shmctl(shmid, IPC_RMID, NULL);
    if (msock)
        close(msock);
    exit(0);
}

/* client 发送信号唤醒server完成通信功能 */
void Server::sigusrHandle(int sig) {
    /* 唤醒子进程信号 */
    if (sig == SIGUSR1) {

        if ((pMesg = (Mesg *)shmat(shmid, NULL, 0)) == (Mesg *) - 1) {
            cerr << "shamt failed " << endl;
            exit(1);
        }

        string cmdMsg = string(pMesg->message[0]);
        string msg = string(pMesg->message[1]);

        if (cmdMsg == "who") thisServer->whoHandle(pMesg->srcSocketFD); 
        else if (cmdMsg == "name") thisServer->nameHandle(pMesg->srcSocketFD, msg);
        else if (cmdMsg == "yell") thisServer->yellHandle(pMesg->srcSocketFD, msg);
        else if (cmdMsg == "tell") thisServer->tellHandle(pMesg->srcSocketFD, msg);
        else if (cmdMsg == "publicPipe") {
            thisServer->preFifoParse(msg, pMesg->srcSocketFD);
        }
        else if (cmdMsg == "broadcast") thisServer->serverBroadcast(msg);
        else if (cmdMsg == "exit") {
            int nowId = thisServer->m_clientPool.findUser(pMesg->srcSocketFD);
            /* 广播离线消息 */
            string logoutMsg = "*** User \'";
            logoutMsg += thisServer->m_clientPool.v_clients[nowId].name;
            logoutMsg += "\' left. ***\n";
            thisServer->serverBroadcast(logoutMsg);

            /* 完成对clientPool 的清理工作 */
            thisServer->m_clientPool.v_clients[nowId].isActive = false;
            close(pMesg->srcSocketFD);
            kill(pMesg->srcPid, SIGUSR2);
            cout << "\t[Info] Close connect" << endl;
        }

        /* 此行一定要加，必须在detach share memory 之前报错pid 信息 */
        int senderPid = pMesg->srcPid;
        /* detach share memory */
        if (shmdt(pMesg) < 0) 
            cerr << "shmdt failed" << endl;
        /* 唤醒信号发送者 
         * 一定要在 share memory 释放之后发送信号！不然会产生对资源的竞争！
         */
        kill(senderPid, SIGUSR2);
    }
    else {}

}


void Server::whoHandle(int fd) {
    int tempFd = dup(1);
    dup2(fd, 1);
    int nowId = m_clientPool.findUser(fd);
    m_clientPool.printUser(nowId);
    dup2(tempFd, 1);
}


void Server::nameHandle(int fd, string nameline) {
    int nowId = m_clientPool.findUser(fd);
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


void Server::yellHandle(int fd, string msgline) {
    int nowId = m_clientPool.findUser(fd);
    string yellMsg = "*** The User ";
    yellMsg += m_clientPool.v_clients[nowId].name;
    yellMsg += " yealed ***: ";
    yellMsg += msgline + "\n";
    serverBroadcast(yellMsg);
}


void Server::tellHandle(int fd, string msgline) {
    int tempStdoutFd = dup(1);
    int tempStderrFd = dup(2);
    dup2(fd, 1);
    dup2(fd, 2);
    int nowId = m_clientPool.findUser(fd);
    /* 找空格 */
    int pos1 = msgline.find_first_of(' ', 0);
    string idMsg = msgline.substr(0, pos1);
    /* 找非空格字符 */
    int pos2 = msgline.find_first_not_of(" \n", pos1);
    /* 找本行结束符 */
    pos1 = msgline.find_first_of("\n", pos2);
    string tellContent = msgline.substr(pos2, pos1 - pos2);
    /* 接收方的id */
    int destId = atoi(idMsg.c_str());
    /* 错误处理给自己发错误信息 */
    if (string::size_type(destId) > m_clientPool.v_clients.size() || m_clientPool.v_clients[destId - 1].isActive == false) {
        /*string tellmsg = "*** Error: User #";*/
        //tellmsg += idMsg;
        //tellmsg += " does not exist yet. ***\n";
        /*sendMessage(nowId, tellmsg);*/
        cout << "*** Error: user #" << idMsg << " does not exist yet. ***" << endl;
        dup2(tempStdoutFd, 1);
        dup2(tempStderrFd, 2);
        return ;
    }
    string tellmsg = "*** ";
    tellmsg += m_clientPool.v_clients[nowId].name;
    tellmsg += " told you ***: ";
    tellmsg += tellContent;
    tellmsg += '\n';
    sendMessage(destId - 1, nowId, tellmsg);

    dup2(tempStdoutFd, 1);
    dup2(tempStderrFd, 2);
}


void Server::copyToShmemory(string cmd, char *message) {
    int size = cmd.length() + 1 > 1024 ? 1023 : cmd.length();

    for (int i = 0; i < size; i++) {
        message[i] = cmd[i];
    }
    message[size] = '\0';
}
