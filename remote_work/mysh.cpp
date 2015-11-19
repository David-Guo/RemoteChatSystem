#include "mysh.h"
#include <vector>
#include <cstddef>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>

bool isUnkCmd;

void Mysh::setEnv(string line) {
    int firstPnt = line.find("PATH");
    int lastPnt = line.find_first_not_of(" ", firstPnt + 4);
    firstPnt = line.find_first_of(" \n", lastPnt);
    PATH = line.substr(lastPnt, firstPnt - lastPnt);
    setenv("PATH", PATH.c_str(), 2);
    //cout << "courent PATH" << PATH << endl;
}

void Mysh::printEnv() {
    cout << "PATH=" << PATH << endl;
}

void Mysh::parseCommand(string s){
    vector<string> commands;
    isExit = false;
    size_t foundPos = 0;
    size_t lastFoundPos = 0;
    string command = "";

    while (1) {
        foundPos = s.find_first_of("\n|!", foundPos);
        if (foundPos == string::npos)  break;

        /* 找|!后面第一个空格或换行 */
        if (s[foundPos] == '|' || s[foundPos] == '!') {
            size_t tempPos = s.find_first_of(" \n", foundPos);
            command  = s.substr(lastFoundPos, tempPos - lastFoundPos);
            commands.push_back(command);
            lastFoundPos = s.find_first_of(" ", foundPos + 1);
            foundPos = lastFoundPos;
        }
        else {
            /* foundPos 已经指到换行 
             * 提取lastFoundPos 到foundPos 即可结束
             */
            command  = s.substr(lastFoundPos, foundPos - lastFoundPos);
            command += '\n';
            commands.push_back(command);
            break;
        }
    }
    isUnkCmd = 0;    
    for (string::size_type i = 0; i < commands.size(); i++) {
        if(commands[i].find("exit") != string::npos) {
            isExit = true;
            return;
        }
        else if(commands[i].find("printenv") != string::npos) {
            printEnv();
            return;
        }
        else if(commands[i].find("setenv") != string::npos) {
            setEnv(commands[i]);
            return;
        }
        //cout << commands[i] << endl;
        pipeToCommand(commands[i]); 
        if (isUnkCmd == 1) break;

        /* 清理掉countDown == 0 的pipe */
        pipevector.eraseInvalidPipe();
    }
    /* 将所有pipe vector中的留下pipe 的countDown减一 */
    pipevector.updateCountDown();

}

bool Mysh::pipeToCommand(string s) {

    int pipefd[2];
    Pipe readPipe;
    int outOrErr;
    /* 为新的进程取得 readpipe 
     * 如果没有找到count = 0 的pipi 则为标准输入
     */
    if (pipevector.getPipetoRead(readPipe)) {
        pipefd[0] = readPipe.readFD;
    }
    else pipefd[0] = 0;
    /* 根据 !|num 为新进程创建写pipe并初始化outOrErr
     * 要考虑写到文件还是写到已有的或是下一个新建的pipe中
     * 还要通过outOrErr向executeProcess 传递标准输出还是错误
     * 从而识别进程重定向输出还是错误到pipdfd[1]
     */
    size_t getPoint = 0;

    if ((getPoint = s.find_first_of('>')) != string::npos) {
        getPoint = s.find_first_not_of(' ', getPoint + 1);
        string pipeFile = s.substr(getPoint, s.find_first_of(" \n", getPoint + 1) - getPoint);
        s = s.substr(0, s.find_first_of('>'));
        pipefd[1] = open(pipeFile.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);// S_IRUSR | S_IWUSR);
        outOrErr = 1;
    }
    else if ((getPoint = s.find_first_of('|')) != string::npos) {
        string downNum = s.substr(getPoint + 1, s.find_first_of(" \n", getPoint + 1) - getPoint - 1);
        if (downNum == "") downNum = "0";
        int countDown = atoi(downNum.c_str());
        outOrErr = 1;
        Pipe writePipe = pipevector.getPipetoWrite(countDown);
        pipefd[1] = writePipe.writeFD;
    }
    else if ((getPoint = s.find_first_of('!')) != string::npos) {
        string downNum = s.substr(getPoint + 1, s.find_first_of(" \n", getPoint + 1) - getPoint - 1);
        if (downNum == "" ) downNum = "0";
        int countDown = atoi(downNum.c_str());
        outOrErr = 2;
        Pipe writePipe = pipevector.getPipetoWrite(countDown);
        pipefd[1] = writePipe.writeFD;
    }
    else {
        pipefd[1] = 1;
        outOrErr = 1;
    }

    return executeProcess(s, readPipe, pipefd, outOrErr);
}
/* outOrErr = 1 说明要重定向标准输出
 * outOrErr = 2 说明要重定向标准错误
 */

bool Mysh::executeProcess(string str, Pipe readPipeToDestroy, int pipefd[2], int outOrErr) {
    /* 从第一个非空字符开始，并去尾符号 */
    int begin = str.find_first_not_of(" ");
    int end = str.find_first_of("|!\n");
    str = str.substr(begin, end - begin);
    str += "\n";

    /* 解析到char **argv中 */
    vector<string> args;
    /* 定位到首个字母 */
    size_t firstPnt = 0; 
    size_t lastPnt = 0;
    string tempStr;
    while (1) {
        /* firstPnt lastPnt 都已经定位到非空字符 */
        firstPnt = str.find_first_of(" \n", lastPnt);
        /* 找到换行符，结束 */
        if (str[firstPnt] == '\n') {
            tempStr = str.substr(lastPnt, firstPnt - lastPnt);
            if (tempStr == "") break;
            args.push_back(tempStr);
            break;
        }
        else {
            tempStr = str.substr(lastPnt, firstPnt - lastPnt);
            if (tempStr == "") break;
            args.push_back(tempStr);
            /* 再次定位到首个非空字符 */
            firstPnt = str.find_first_not_of(" ", firstPnt + 1);
            lastPnt = firstPnt;
        }
    }

    char **argv = new char*[args.size() + 1];
    for (string::size_type i = 0; i < args.size(); i++) {
        argv[i] = new char[args[i].length() + 1];
        for (string::size_type k = 0; k < args[i].length(); k++)
            argv[i][k] = args[i][k];
        argv[i][args[i].length()] = '\0';
    }
    argv[args.size()] = NULL;

    //signal(SIGPIPE, SIG_DFL);
    /* 此处必须要加，如果不是从标准输入读数据，必须要关掉该pipe 的写方向 */
    if (pipefd[0] != 0)
        close(readPipeToDestroy.writeFD);

    /* fork 子进程执行指令 */
    int childpid;
    childpid = fork();

    if (childpid < 0) {
        cerr << "Fork failed" << endl;
        return 0;
    }
    else if (childpid == 0) {
/*        cout << "exec read  pipe: " <<  pipefd[0] << endl;*/
        /*cout << "exec write pipe: " <<  pipefd[1] << endl;*/
        dup2(pipefd[0], 0);
        dup2(pipefd[1], outOrErr);
        if (pipefd[0] != 0) close(pipefd[0]);
        if (pipefd[1] != outOrErr) close(pipefd[1]);
        int Status = execvp(argv[0], argv);
        if (Status == -1) {
            cerr << "Unknown command: [" << args[0] << "]." << endl;
        }
        exit(1);
    }
    else {
        int Status;
        if (wait(&Status) < 0) {
            //cout << "error status: " << Status << endl;
            return false;
        }
        else  {
            /* 子进程执行unkcmd 时wait 得到的status 值为256 */
            if (Status != 0) {
                pipevector.errorHandle(readPipeToDestroy, pipefd, outOrErr);
                /* 直接结束本行命令的跌代，在commands 处检查为1则结束loop */
                isUnkCmd = 1;
                return true;
            }
            /* 注意此处不能关掉 write pipe 否则多个pipe 不会合并!!! */
            //if (pipefd[1] != 1) close(pipefd[1]);
            /* 出现错误命令时不能关掉读 pipe !!! */
            if (pipefd[0] != 0) close(pipefd[0]);
            //cout << "right status: " << Status << endl;
            isUnkCmd = 0;

            return true;
        }
    }

    for (string::size_type i = 0; i < args.size(); i++) {
        cout << argv[i] << endl;
        delete(argv[i]);
    }
    return true;
}


//int main() {

    //[>if (chdir("./ras") != 0) {<]
    ////cerr << "Change dirctory error" << endl;
    ////exit(1);
    //[>}<]

    //Mysh shell;
    //shell.setEnv("setenv PATH bin:.");
    //shell.printEnv();
    //for ( ; ; ) {	
        //string tempStr;
        //std::getline(std::cin,tempStr);

        //tempStr += "\n";
        //shell.parseCommand(tempStr);
        //if(shell.isExit)
            //break;
        //[> 换成close(socker) <]
        //cout << "% " << flush;

    //}

    //return 0;
//}

