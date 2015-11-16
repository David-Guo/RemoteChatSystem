#ifndef _PIPEVECTOR_H_
#define _PIPEVECTOR_H_

#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

class Pipe{
    public:
        int writeFD;
        int readFD;
        bool eraseFlag;
        bool inThisLineFlag;
        int countDown;
};
/* inThisLineFlag = 1 说明为这一行指令的 pipe 
 * 在getPipetoWrite() 中初始化为1
 */

class PipeVector{
    public:
        bool getPipetoRead(Pipe &pipeToCopy);
        Pipe & getPipetoWrite(int countDown);
        void updateCountDown();
        void eraseInvalidPipe();
        void errorHandle(Pipe &pipeToDestory, int pipefd[2], int ourOrErr);

    private:
        vector<Pipe> v_pipe;
};


//bool PipeVector::getPipetoRead(Pipe &pipeToCopy) {
    //for (string::size_type i = 0; i < v_pipe.size(); i++) {
        //if (v_pipe[i].countDown == 0) {
            //v_pipe[i].eraseFlag = 1;
            //pipeToCopy = v_pipe[i];
            //return true;
        //}
    //}
    //return false;
//}

//Pipe &PipeVector::getPipetoWrite(int countDown) {
    //[> 找到含有相同的countDown pipe 并返回 <]
    //for (string::size_type i = 0; i < v_pipe.size(); i++) {
        //if (v_pipe[i].countDown == countDown && countDown != 0) {
            //if (v_pipe[i].inThisLineFlag ==1 ) 
                //continue;
            //else    
                //return v_pipe[i];
        //}
    //}

    //[> 找不到则创建新的pipi <]
    //int pipefd[2];
    //if (pipe(pipefd) < 0) {
        //cerr << "creat pipe error" << endl;
        //exit(1);
    //}
    //cout << "new readpipe: " << pipefd[0] << endl;
    //cout << "new writepipr: " << pipefd[1] << endl;

    //Pipe newPipe;
    //newPipe.inThisLineFlag = 1;
    //newPipe.eraseFlag = 0;
    //newPipe.countDown = countDown;
    //newPipe.readFD = pipefd[0];
    //newPipe.writeFD = pipefd[1];
    //v_pipe.push_back(newPipe);

    //return v_pipe[v_pipe.size() - 1];
//}


//[> 清除掉 eraseFlag  == 1 的pipe <]
//void PipeVector::eraseInvalidPipe() {
    //for (string::size_type i = 0; i < v_pipe.size(); i++) {
        //if (v_pipe[i].eraseFlag == 1) 
            //v_pipe.erase(v_pipe.begin() + i);
    //}
//}
        

//void PipeVector::updateCountDown() {
    //for (string::size_type i = 0; i < v_pipe.size(); i++) {
        //v_pipe[i].countDown--;
        //v_pipe[i].inThisLineFlag = 0;
    //}
//}


//void PipeVector::errorHandle(Pipe &pipeToDestory, int pipefd[2], int outOrErr) {
    //for (string::size_type i = 0; i < v_pipe.size(); i++) {
        //[> 未知命令读pipe 的eraseFlag 与countDown 值需要恢复 <]
        //if (v_pipe[i].readFD == pipeToDestory.readFD && v_pipe[i].inThisLineFlag == 0) {
            //v_pipe[i].eraseFlag = 0;
            //v_pipe[i].countDown++;
        //}
        //[> 未知命令创建了新的pipe 将其擦除 <]
        //if (v_pipe[i].writeFD == pipefd[1] && pipefd[1] != outOrErr)
            //v_pipe.erase(v_pipe.begin() + i);
        
    //}
    //cout << "v_pipe num: " << v_pipe.size() << endl;
//}

#endif
