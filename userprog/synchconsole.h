#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "copyright.h"
#include "console.h"
#include "synch.h"

class SynchConsole {
  public:
    SynchConsole(char *readFile, char *writeFile);
    ~SynchConsole();
    char GetChar();
    void PutChar(char ch);

    Semaphore *readAvail;
    Semaphore *writeDone;

  private:
    Console *console;
    Lock *lock;
    
    
};

#endif // SYNCHCONSOLE_H