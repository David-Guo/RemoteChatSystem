#include <iostream>
using namespace std;

#define FILENUM 31

class Fifo {

    public: 
        int v_fd[FILENUM];
        int v_readState[FILENUM];
        int v_writeState[FILENUM];
        string v_fileName[FILENUM];


    public:
        Fifo();
        ~Fifo();
};


