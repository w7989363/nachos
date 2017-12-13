#include "syscall.h"

//Fork Yield
void func(){
    Create("bbb.txt");
    Exit(0);
}
int main(){
    Fork(func);
    Yield();
    Exit(0);

}

//Create Open Write Read Close
// int main(){
//     int id,num;
//     char buffer[128];
//     Create("aaa.txt");
//     id = Open("aaa.txt");
//     Write("abcdefghij",10,id);
//     num = Read(buffer, 5, id);
//     Close(id);
// }

//Exec Join Exit
// int main(){
//     int id;
//     id = Exec(".\/test\/test1");
//     Join(id);
//     Exit(0);
//     Halt();
// }