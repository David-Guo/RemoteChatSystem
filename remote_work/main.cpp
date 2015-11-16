#include "server.h"


int main(int argc, char *argv[]) {

    if (argc != 2) {
        cerr << "Usage: main [port]" << endl;
        exit(1);
    }
    string port(argv[1], strlen(argv[1]));

    Server m_server(port);


    return 0;

}
