#include "copyright.h"
#include "synchconsole.h"
#include "system.h"


static void ReadAvail(int arg) { 
    SynchConsole *syncon = (SynchConsole*)arg;
    syncon->readAvail->V(); 
}
static void WriteDone(int arg) { 
    SynchConsole *syncon = (SynchConsole*)arg;
    syncon->writeDone->V(); 
}

SynchConsole::SynchConsole(char *readFile, char *writeFile){
    console = new Console(readFile, writeFile, ReadAvail, WriteDone, (int) this);
    lock = new Lock("lock");
    readAvail = new Semaphore("read", 0);
    writeDone = new Semaphore("write", 0);
}

SynchConsole::~SynchConsole(){
    delete console;
    delete lock;
    delete readAvail;
    delete writeDone;
}

char SynchConsole::GetChar(){
    char ch;
    //加读锁，允许一个进程访问
    lock->Acquire();
    //等待读取字符
    readAvail->P();
    ch = console->GetChar();
    //释放读锁
    lock->Release();
    return ch;
}

void SynchConsole::PutChar(char ch){
    //加写锁，允许一个进程访问
    lock->Acquire();
    //写
    console->PutChar(ch);
    //等待写完成
    writeDone->P();
    //释放写锁
    lock->Release();
}