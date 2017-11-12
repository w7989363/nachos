#include "syscall.h"

#define M 6
#define N 6
int num[M][N];
int main(){
    int i,j;
    for(i=0;i<M;i++){
        for(j=0;j<N;j++){
            num[i][j] = i*j;
        }
    }
    Exit(1);
    //Halt();
}