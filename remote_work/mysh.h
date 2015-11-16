#ifndef _MYSH_H_
#define _MYSH_H_

#include <cstring>
#include <iostream>
#include "pipeVector.h"

using namespace std;


/* 有一个pipevector 类成员但只能通过该类的函数操作 v_pipe
 * 无法直接访问 */

class Mysh{
    public:

        PipeVector pipevector;
        string PATH;
        bool isExit;

        void parseCommand(string line);
        bool pipeToCommand(string line);
        bool executeProcess(string str, Pipe readPipeToDestroy, int pipefd[2], int outOrErr);
        void setEnv(string line);
        void printEnv();


};

#endif
