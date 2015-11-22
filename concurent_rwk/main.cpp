#include "server.h"


int main(int argc, char *argv[]) {

   /* if (argc != 2) {*/
        //cerr << "Usage: main [port]" << endl;
        //exit(1);
    /*}*/

    if (chdir ("./ras") != 0) {
        cerr << "Change working directory failed" << endl;
    }

    string port(argv[1], strlen(argv[1]));

    Server m_server(port);

    return 0;

}
