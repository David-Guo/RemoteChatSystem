#include <cassert>
#include "clientPool.h"

ClientNode::ClientNode(int ssock, string m_name, string m_ip, string m_port)
{
    sock = ssock;
    name = m_name;
    ip   = m_ip;
    port = m_port;
    isActive = true;
    PATH = "bin:.";
    mysh = new Mysh;
}
ClientNode::~ClientNode() {;}

void ClientNode::init(int ssock, string m_name, string m_ip, string m_port){
sock = ssock;
name = m_name;
ip   = m_ip;
port = m_port;
isActive = true;
PATH = "bin:.";
}

ClientPool::ClientPool() {;}


ClientPool::~ClientPool() {;}

/* 添加用户，在msock accept接收到新请求时调用 */
int ClientPool::addUser(int ssock, string ip, string port) {
/* 遍历查找无效用户id */
for (string::size_type i = 0; i < v_clients.size(); i++){
    if (v_clients[i].isActive == false) {
        v_clients[i].init(ssock, string("no mane"), ip, port);
        return i;
    }
}
/* 如果没有找到 */
ClientNode newNode = ClientNode(ssock, string("no name"), ip, port);
v_clients.push_back(newNode);
return v_clients.size() - 1;
}

/* 用户id 与fd 之间的映射 */
int ClientPool::findUser(int fd) {
for (string::size_type i = 0; i < v_clients.size(); i++)
    if (v_clients[i].sock == fd)
        return i;

cerr << "Can't find the fd:" << fd << endl;
exit(1);
}


/* 根据id 取得用户shell */
Mysh * ClientPool::getShell(int id) {
assert(id >= v_clients.size());
assert(v_clients[id].isActive == false);

return v_clients[id].mysh;
}

void ClientPool::printUser(int id) {
cout << "<ID>\t<nickname>\t<IP/port>\t<indicate me>" << endl;
for (string::size_type i = 0; i < v_clients.size(); i++) {
    if (v_clients[i].isActive == true) {
        cout << i+1 << '\t';
        cout << v_clients[i].name << '\t';
        cout << v_clients[i].ip << '/' << v_clients[i].port << "\t\t";
        if (i == string::size_type(id)) cout << "<-me" << endl;
        else cout << endl;
        }
    }
}


void ClientPool::changeName(int id, string line) {
        if (string::size_type(id) < v_clients.size() && v_clients[id].isActive == true)
            v_clients[id].name = line;
        else {
            cerr << "id is lager than clients size" << endl;
            exit(1);
        }

}

            

