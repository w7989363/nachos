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
    printf("thread %d running. uid=%d\n",currentThread->getTid(),currentThread->getUid());
    currentThread->Yield();
    // for (num = 0; num < 5; num++) {
    //     //int a[100000];
    //     printf("*** thread %d looped %d times\n", which, num);
    //     printf("thread id: %d\n",currentThread->getTid());
    //     printf("thread name: %s\n", currentThread->getName());
    //     currentThread->Yield();
    // }
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

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);

    Thread *t1 = new Thread("forked thread1");
    
    t1->Fork(SimpleThread, (void*)2);
    SimpleThread(0);
}

void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest1");

    int createNum = 127;
    for(int i=0;i<createNum;i++){
        Thread *t = new Thread("thread");
        t->Fork(SimpleThread, (void*)1);
    }
    
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
    case 2:
    ThreadTest2();
	break;
    default:
	printf("No test specified.\n");
	break;
    }
}

