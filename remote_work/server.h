#include "clientPool.h"
#include "fifo.h"


class Server {
    public:
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
        
        bool preFifoParse(string cmdline, int nowId, int &readFD, int& writeFD);
        void pipeCharEarse(string &cmdline);

        
    public:
        ClientPool m_clientPool;
        Fifo m_fifo;
        

};


