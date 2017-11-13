// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"


//FIFO置换算法
int FIFOReplace(){
    for(int i = 0; i < TLBSize-1; i++){
        machine->tlb[i] = machine->tlb[i+1];
    }
    return TLBSize-1;
}

//LRU置换算法
int LRUReplace(){
    //挑出LRU_mark最大的对应项
    int pos = 0;
    int max = -1;
    for(int i = 0; i < TLBSize; i++){
        if(machine->LRU_mark[i]>max){
            max = machine->LRU_mark[i];
            pos = i;
        }
    }
    //将LRU_mark对应项置0
    machine->LRU_mark[pos] = 0;
    return pos;
}

//页表却页处理
void UpdatePageTable(){
    //虚拟页号，加上偏移对应虚存中该页位置
    int vpn = (unsigned)machine->ReadRegister(BadVAddrReg) / PageSize;
    //bitmap分配一页物理内存
    int ppn = machine->bitmap->Find();
    //物理内存满
    ASSERT(ppn != -1);
    //printf("ppn:%d\n",ppn);
    //拷贝到物理内存
    for(int i = 0; i < PageSize; i++){
        //printf("mainmem:%d,swap:%d\n",ppn*PageSize + i,(vpn+currentThread->space->vpnoffset)*PageSize + i);
        machine->mainMemory[ppn*PageSize + i] = machine->swapspace[(vpn+currentThread->space->vpnoffset)*PageSize + i];
    }
    //修改页表
    machine->pageTable[vpn].physicalPage = ppn;
    machine->pageTable[vpn].valid = TRUE;
    printf("vpn:%d has been inserted into mainMem:%d\n\n",vpn,ppn);
}

//TLB缺页处理
void UpdateTLB(){
    int vpn = (unsigned)machine->ReadRegister(BadVAddrReg) / PageSize;
    printf("vpn:%d\n",vpn);
    int pos = -1;
    for(int i = 0; i<TLBSize; i++){
        if(machine->tlb[i].valid == FALSE){
            pos = i;
            break;
        }
    }
    //TLB满，执行替换算法
    if(pos = -1){
        //pos = FIFOReplace();
        pos = LRUReplace();
    }
    //页表该项valid为false
    if(!machine->pageTable[vpn].valid){
        //却页处理程序，将该页调入内存
        printf("PageFault. reading from swap.\n");
        UpdatePageTable();
    }
    //该页已经调入内存
    ASSERT(machine->pageTable[vpn].valid);
    //插入TLB
    machine->tlb[pos].valid = TRUE;
    machine->tlb[pos].virtualPage = vpn;
    machine->tlb[pos].physicalPage = machine->pageTable[vpn].physicalPage;
    machine->tlb[pos].use = FALSE;
    machine->tlb[pos].dirty = FALSE;
    machine->tlb[pos].readOnly = FALSE;
    
}


//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if (which == SyscallException) {
        switch(type){
        case SC_Halt:
            DEBUG('a', "Shutdown, initiated by user program.\n");
   	        interrupt->Halt();
            break;
        case SC_Exit:
            DEBUG('a', "user program is done.\n");
            //printf("ret:%d",machine->ReadRegister(4));
            currentThread->Finish();
            break;
        default:
            printf("Unexpected user mode exception %d %d\n", which, type);
	        ASSERT(FALSE);
        }
    }
    else if(which == PageFaultException){
        //TLB缺页处理
        printf("TLBPageFaultException,reading from pageTable.\n");
        UpdateTLB();  
    }
    else {
	    printf("Unexpected user mode exception %d %d\n", which, type);
	    ASSERT(FALSE);
    }
}

