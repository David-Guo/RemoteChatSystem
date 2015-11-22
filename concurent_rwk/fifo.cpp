#include "fifo.h"

Fifo::Fifo() {
    for (int i = 0; i < FILENUM; i++) {
        string fifoname = "../temp/fifo";
        fifoname += to_string(i);
        fifoname += ".txt";
        
        v_fileName[i] = fifoname;
        v_readState[i] = 0;
        v_writeState[i] = 0;
        v_fd[i] = i;
    }

}

Fifo::~Fifo() {};        
