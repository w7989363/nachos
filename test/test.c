#include "syscall.h"

#define M 20
#define N 20
int num[M][N];
int main(){
    int i,j;
    for(i=0;i<M;i++){
        for(j=0;j<N;j++){
            num[i][j] = i*j;
        }
    }
    // Exit();
    Halt();
}