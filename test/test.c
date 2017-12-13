#include "syscall.h"

int main(){
    int id,num;
    char buffer[128];
    Create("aaa.txt");
    id = Open("aaa.txt");
    Write("abcdefghij",10,id);
    num = Read(buffer, 5, id);
    Close(id);
}

//Exec Join Exit
// int main(){
//     int id;
//     id = Exec(".\/test\/test1");
//     Join(id);
//     Exit(0);
//     Halt();
// }