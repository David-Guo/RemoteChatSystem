#include "clientPool.h"


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

        
    public:
        ClientPool m_clientPool;

};


