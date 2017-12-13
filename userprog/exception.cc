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
#include "openfile.h"
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

//页表缺页处理
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
        //从虚存中读进内存
        machine->mainMemory[ppn*PageSize + i] = machine->swapspace[(vpn+currentThread->space->vpnoffset)*PageSize + i];
    }
    //修改页表
    machine->pageTable[vpn].physicalPage = ppn;
    machine->pageTable[vpn].valid = TRUE;
    printf("[exception]thread (%s) vpn (%d) has been inserted into mainMem (%d)\n",currentThread->getName(),vpn,ppn);
}

//TLB缺页处理
void UpdateTLB(){
    int vpn = (unsigned)machine->ReadRegister(BadVAddrReg) / PageSize;
    //printf("vpn:%d\n",vpn);
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
        //printf("PageFault. reading from swap.\n");
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

//join子线程函数
void exec_fork_func(int name){
    char *filename = new char[128];
    filename = (char*)name;
    printf("[exception]Starting userprog (%s)\n",filename);
    //打开可执行文件
    OpenFile *file = fileSystem->Open(filename);
    ASSERT(file != NULL);
    //给该文件创建地址空间
    AddrSpace *space = new AddrSpace(file);
    //地址空间赋给当前线程
    currentThread->space = space;
    //初始化系统寄存器
    space->InitRegisters();
    //把地址空间中页表设置为当前页表
    space->RestoreState();
    printf("[exception]userprog (%s) is running\n",filename);
    machine->Run();
}

//Exec系统调用
void SyscallExec(){
    int base = machine->ReadRegister(4);
    int value;
    int count;
    int i;
    char *para = new char[128];
    //逐个字节读取内存base地址中的参数
    count = 0;
    do{
        machine->ReadMem(base++, 1, &value);
        printf("%d\n",value);
        count++;
    }while(value != 0);
    base = base-count;
    for(i=0;;i++){
        machine->ReadMem(base+i,1,&value);
        para[i] = (char)value;
        if(para[i] == '\0'){
            break;
        }
    }
    //新创建线程
    Thread *newthread = new Thread("childThread1",0);
    //在当前线程的子县城数组中加入该子线程
    bool found = false;
    for(count=0;count<MaxChildThreadNum;count++){
        if(currentThread->childThread[count] == NULL){
            currentThread->childThread[count] = newthread;
            //将返回值写入2号寄存器
            machine->WriteRegister(2,(int)newthread);
            found = true;
            break;
        }
    }
    //子线程数组满
    if(!found){
        printf("[exception]full of children. Exec failed.\n");
        machine->PCAdvanced();
        return;
    }
    //更改子线程的父线程
    newthread->fatherThread = currentThread;
    printf("%s\n",para);
    newthread->Fork(exec_fork_func, (int)para);
    //delete para;
    machine->PCAdvanced();
}

//Join系统调用
void SyscallJoin(){
    //读取要等待的子线程
    int id = machine->ReadRegister(4);
    Thread *cthread = (Thread*)id;
    bool found = false;
    int num = -1;
    //在子线程数组中寻找该线程
    for(int i = 0; i<MaxChildThreadNum;i++){
        if(currentThread->childThread[i] == cthread){
            num = i;
            found = true;
            break;
        }
    }
    //没有找到该子线程
    if(!found){
        printf("[exception]cannot find child thread (%s). Join failed.\n",cthread->getName());
        return;
    }
    //找到了该线程并且子线程未结束
    while(num != -1 && currentThread->childThread[num] != NULL ){
        printf("[exception]thread (%s) is waiting for his child (%s) \n",currentThread->getName(),cthread->getName());
        currentThread->Yield();
    }
    printf("[exception]child thread has been finished. Join success.\n");
    //ExitCode写回2号寄存器
    machine->WriteRegister(2,0);

    machine->PCAdvanced();
}

//Create系统调用
void SyscallCreate(){
    int base = machine->ReadRegister(4);
    int value;
    int count;
    int i;
    char *para = new char[128];
    //逐个字节读取内存base地址中的参数
    count = 0;
    do{
        machine->ReadMem(base++, 1, &value);
        count++;
    }while(value != 0);
    base = base-count;
    for(i=0;i<count;i++){
        machine->ReadMem(base+i,1,&value);
        para[i] = (char)value;
    }
    printf("%s\n",para);
    if(fileSystem->Create(para, 128, 0, "")){
        printf("[exception]create file (%s) succeed.\n",para);
    }
    else{
        printf("[exception]create file (%s) failed.\n",para);
    }
    delete para;
    machine->PCAdvanced();
}

//Open系统调用
void SyscallOpen(){
    int base = machine->ReadRegister(4);
    int value;
    int count;
    int i;
    char *para = new char[128];
    //逐个字节读取内存base地址中的参数
    count = 0;
    do{
        machine->ReadMem(base++, 1, &value);
        count++;
    }while(value != 0);
    base = base-count;
    for(i=0;i<count;i++){
        machine->ReadMem(base+i,1,&value);
        para[i] = (char)value;
    }
    //调用文件系统接口打开文件
    OpenFile *file = fileSystem->Open(para);
    if(file != NULL){
        printf("[exception]open file (%s) succeed. id (%d)\n",para, (int)file);
        //返回参数写入2号寄存器
        machine->WriteRegister(2,(int)file);
    }
    else{
        printf("[exception]open file (%s) failed.\n",para);
        machine->WriteRegister(2,0);
    }
    delete para;
    machine->PCAdvanced();
}

//Close系统调用
void SyscallClose(){
    int value = machine->ReadRegister(4);
    OpenFile *file = (OpenFile*)value;
    printf("[exception]closing file id (%d)\n",value);
    delete file;
    machine->PCAdvanced();
}

//Write系统调用
void SyscallWrite(){
    int bufferbase = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);
    int fd = machine->ReadRegister(6);
    char *contents = new char[128];
    int i,value;
    //获取buffer内容
    for(i=0;i<size;i++){
        machine->ReadMem(bufferbase+i, 1, &value);
        contents[i] = (char)value;
    }
    contents[i] = '\0';

    OpenFile* file = (OpenFile*)fd;
    if(file == NULL){
        printf("[exception]write file id is null. Write failed\n");
    }
    else{
        printf("[exception]writing contents (%s)\n",contents);
        //写文件
        file->Write(contents, size);
    }
    delete contents;
    machine->PCAdvanced();
}

//Read系统调用
void SyscallRead(){
    int bufferbase = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);
    int fd = machine->ReadRegister(6);
    char *contents = new char[128];
    int i,value;
    OpenFile* openfile = (OpenFile*)fd;
    if(openfile == NULL){
        printf("[exception]read file id is null. Read failed\n");
    }
    else{
        printf("[exception]reading (%d) bytes from file to buffer\n",size);
        //从文件读进缓冲区
        i = openfile->Read(contents, size);
        contents[size] = '\0';
        //读出字节数写回2号寄存器
        machine->WriteRegister(2, i);
        //printf("[exception]contents (%s)\n",contents);
        for(i = 0; i < size; i++){
            value = (int)contents[i];
            machine->WriteMem(bufferbase+i, 1, value);
        }
        //字符串结尾写入内存
        machine->WriteMem(bufferbase+i, 1, 0);
        printf("[exception]write contents to Nachos mainMemory\n");
    }
    delete contents;
    machine->PCAdvanced();
}

void fork_func(int pc){
    printf("[exception]child thread fork\n");
    //设置子线程空间
    AddrSpace* sp = currentThread->fatherThread->space;
    AddrSpace* space = new AddrSpace(sp);
    currentThread->space = space;
    //设置PC
    machine->WriteRegister(PCReg, pc);
    machine->WriteRegister(NextPCReg, pc+4);
    //保存线程状态
    printf("[exception]child thread start running.\n");
    machine->Run();
}
//Fork系统调用
void SyscallFork(){
    int i;
    //读取子线程函数的指令地址
    int PC = machine->ReadRegister(4);
    //创建子线程
    Thread *cthread = new Thread("childThread2", 0);
    bool found = false;
    //子线程添加到当前线程的子线程数组中
    for(i = 0; i < MaxChildThreadNum; i++){
        if(currentThread->childThread[i] == NULL){
            currentThread->childThread[i] = cthread;
            found = true;
            break;
        }
    }
    if(!found){
        printf("[exception]full of children. Exec failed.\n");
        machine->PCAdvanced();
        return;
    }
    //子线程的父线程赋值
    cthread->fatherThread = currentThread;
    cthread->Fork(fork_func, PC);

    machine->PCAdvanced();
}


//Yield系统调用
void SyscallYield(){
    machine->PCAdvanced();
    printf("[exception]thread (%s) yield.\n",currentThread->getName());
    currentThread->Yield();
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
            int code;
            code = machine->ReadRegister(4);
            printf("[exception]thread (%s) exit. Code (%d)\n",currentThread->getName(),code);
            currentThread->Finish();
            machine->PCAdvanced();
            break;
        case SC_Exec:
            SyscallExec();
            break;
        case SC_Join:
            SyscallJoin();
            break;
        case SC_Create:
            SyscallCreate();
            break;
        case SC_Open:
            SyscallOpen();
            break;
        case SC_Close:
            SyscallClose();
            break;
        case SC_Write:
            SyscallWrite();
            break;
        case SC_Read:
            SyscallRead();
            break;
        case SC_Fork:
            SyscallFork();
            break;
        case SC_Yield:
            SyscallYield();
            break;
        default:
            printf("Unexpected user mode exception %d %d\n", which, type);
	        ASSERT(FALSE);
        }
    }
    else if(which == PageFaultException){
        //TLB缺页处理
        //  printf("TLBPageFaultException,reading from pageTable.\n");
        UpdateTLB();  
    }
    else {
	    printf("Unexpected user mode exception %d %d\n", which, type);
	    ASSERT(FALSE);
    }
}

