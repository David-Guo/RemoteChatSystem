#include <vector>
#include "mysh.h"

class ClientNode {

    public:
        ClientNode(int sock, string name, string ip, string port);
        void init(int ssock, string name, string ip, string port);
        ~ClientNode();
    
    public:
        string name;
        string ip;
        string port;
        string PATH;
        int sock;
        bool isActive;
        Mysh *mysh;
};

class ClientPool {
        
    public:
        ClientPool();
        ~ClientPool();
        int addUser(int ssock, string ip, string port);
        int findUser(int ssock);
        Mysh *getShell(int id);
        void printUser(int id);
        void changeName(int id, string line);

    public:
        vector<ClientNode> v_clients;
};

