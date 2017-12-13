#include "syscall.h"

int main(){
    int id;
    id = Exec(".\/test\/test1");
    Join(id);
    Exit(0);
    Halt();
}