#include "pipeVector.h"

bool PipeVector::getPipetoRead(Pipe &pipeToCopy) {
    for (string::size_type i = 0; i < v_pipe.size(); i++) {
        if (v_pipe[i].countDown == 0) {
            v_pipe[i].eraseFlag = 1;
            pipeToCopy = v_pipe[i];
            return true;
        }
    }
    return false;
}

Pipe &PipeVector::getPipetoWrite(int countDown) {
    /* 找到含有相同的countDown pipe 并返回 */
    for (string::size_type i = 0; i < v_pipe.size(); i++) {
        if (v_pipe[i].countDown == countDown && countDown != 0) {
            if (v_pipe[i].inThisLineFlag ==1 ) 
                continue;
            else    
                return v_pipe[i];
        }
    }

    /* 找不到则创建新的pipi */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        cerr << "creat pipe error" << endl;
        exit(1);
    }
    /*cout << "new readpipe: " << pipefd[0] << endl;*/
    /*cout << "new writepipr: " << pipefd[1] << endl;*/

    Pipe newPipe;
    newPipe.inThisLineFlag = 1;
    newPipe.eraseFlag = 0;
    newPipe.countDown = countDown;
    newPipe.readFD = pipefd[0];
    newPipe.writeFD = pipefd[1];
    v_pipe.push_back(newPipe);

    return v_pipe[v_pipe.size() - 1];
}


/* 清除掉 eraseFlag  == 1 的pipe */
void PipeVector::eraseInvalidPipe() {
    for (string::size_type i = 0; i < v_pipe.size(); i++) {
        if (v_pipe[i].eraseFlag == 1) 
            v_pipe.erase(v_pipe.begin() + i);
    }
}
        

void PipeVector::updateCountDown() {
    for (string::size_type i = 0; i < v_pipe.size(); i++) {
        v_pipe[i].countDown--;
        v_pipe[i].inThisLineFlag = 0;
    }
}


void PipeVector::errorHandle(Pipe &pipeToDestory, int pipefd[2], int outOrErr) {
    /* 所有pipe countdown 值加一 */
    for (string::size_type i = 0; i < v_pipe.size(); i++)
        v_pipe[i].countDown++;

    /* 不是从标准输入或本行pipe 接收输入的错误命令需要创建新pipe */
    if (pipeToDestory.readFD != 0 && pipeToDestory.inThisLineFlag == 0) {
        /* 创建新的pipe 替换即将删除的 */
        int newpipefd[2];
        if (pipe(newpipefd) < 0) {
            cerr << "cannt creat pepe" << endl;
            exit(1);
        }
        /*cout << "nuw readpipe: " << newpipefd[0] << endl;*/
        /*cout << "nuw writepipe: " << newpipefd[1] << endl;*/
        Pipe tempipe;
        char dataBuffer[10000];
        int sizeOfData = read(pipefd[0], dataBuffer, sizeof(dataBuffer));
        if (sizeOfData < 0)  cout << "size :" << sizeOfData << endl;
        if (sizeOfData != write(newpipefd[1], dataBuffer, sizeOfData)) cerr << "write to new error" << endl;
        /* 新pipe 的初始化 */
        tempipe.writeFD = newpipefd[1];
        tempipe.readFD = newpipefd[0];
        tempipe.eraseFlag = 0;
        tempipe.countDown = 1;
        v_pipe.push_back(tempipe);

        /* 删掉读过的Pipe */
        for (string::size_type i = 0; i < v_pipe.size(); i++) {
            /* 未知命令且不是本行产生的读pipe 的eraseFlag 与countDown 值需要恢复 */
            if (v_pipe[i].readFD == pipeToDestory.readFD)
                v_pipe.erase(v_pipe.begin() + i);
        }
    }

    /* 未知命令创建了新的pipe 将其擦除 */
    if (pipefd[1] != outOrErr)
        for (string::size_type i = 0; i < v_pipe.size(); i++)
            if (v_pipe[i].writeFD == pipefd[1])
                v_pipe.erase(v_pipe.begin() + i);

    //cout << "v_pipe num: " << v_pipe.size() << endl;
}
