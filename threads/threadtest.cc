// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
        //int a[100000];
        printf("*** thread %d looped %d times. pri = %d\n", currentThread->getTid(), num, currentThread->getPri());
        currentThread->Yield();
    }
}

void PrintThread(){
    printf("thread %d running. uid=%d\n",currentThread->getTid(),currentThread->getUid());
    currentThread->Yield();
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t1 = new Thread("forked thread1", 3);

    t1->Fork(SimpleThread, (void*)1);

    Thread *t2 = new Thread("forked thread2",1);
    
    t2->Fork(SimpleThread, (void*)1);

    Thread *t3 = new Thread("forked thread2",2);
    
    t3->Fork(SimpleThread, (void*)1);

}

//最大线程测试
void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest1");

    int createNum = 127;
    for(int i=0;i<createNum;i++){
        Thread *t = new Thread("thread");
        t->Fork(PrintThread, NULL);
    }
    
}

//TS函数
void ThreadShow(){
    printf("tid          uid          name\n");
    for(int i=0; i<MAXTHREAD; i++){
        if(t_pointer[i]!=NULL){
            printf("%3d%11d%18s\n",t_pointer[i]->getTid(),t_pointer[i]->getUid(),t_pointer[i]->getName());
        }
    }
}

//TS测试
void
ThreadTest3()
{
    DEBUG('t', "Entering ThreadTest1");
    //创建10个进程
    int createNum = 10;
    for(int i=0;i<createNum;i++){
        Thread *t = new Thread("thread");
        t->Fork(SimpleThread, (void*)1);
    }
    
    //调用TS显示进程
    ThreadShow();
}



//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
        ThreadTest1();
        break;
    case 2:
        ThreadTest2();
        break;
    case 3:
        ThreadTest3();
	    break;
    default:
	    printf("No test specified.\n");
	    break;
    }
}

