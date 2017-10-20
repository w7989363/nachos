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
#include "list.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 1;
//信号量
Semaphore* s_full;
Semaphore* s_empty;
Semaphore* s_mutex;
//缓存数据
List* list;
//产品
typedef struct product{
    int value;
}Product;

//锁
Lock* lock;
//条件变量
Condition* c_empty;
Condition* c_full;


void PrintThread(){
    printf("thread %d running. uid=%d\n",currentThread->getTid(),currentThread->getUid());
    currentThread->Yield();
}

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------
void SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
        // if(num == 3){
        //     Thread* t = new Thread("higher thread",0);
        //     t->Fork(PrintThread,1);
        // }
        //int a[100000];
        printf("*** thread %d looped %d times. pri = %d\n", currentThread->getTid(), num, currentThread->getPri());
        currentThread->Yield();
    }
}



//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void ThreadTest1()
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
void ThreadTest2()
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
void ThreadTest3()
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

//生产者函数 信号量
void producerWithSemaphore(){
    for(int i = 0; i < 20; i++){
        s_empty->P();
        s_mutex->P();
        Product* pro = new Product;
        pro->value = i;
        list->Append(pro);
        printf("thread %s produce : %d. buffer:%d/10\n",currentThread->getName(),i,list->NumInList());
        s_full->V();
        s_mutex->V();
    }
}
//消费者函数 信号量
void consumerWithSemaphore(){
    for(int i = 0; i < 10; i++){
        s_full->P();
        s_mutex->P();
        Product* pro = (Product*)list->Remove();
        printf("thread %s consume : %d. buffer:%d/10\n",currentThread->getName(),pro->value,list->NumInList());
        delete pro;
        s_empty->V();
        s_mutex->V();
    }
}
//信号量解决生产者消费者问题
void ThreadTest4(){
    s_full = new Semaphore("full", 0);
    s_empty = new Semaphore("empty", 10);
    s_mutex = new Semaphore("mutex", 1);
    list= new List;
    Thread* pro1 = new Thread("producer");
    pro1->Fork(producerWithSemaphore,(void*)1);
    Thread* con1 = new Thread("consumer1");
    con1->Fork(consumerWithSemaphore,(void*)1);
    Thread* con2 = new Thread("consumer2");
    con2->Fork(consumerWithSemaphore,(void*)1);

}

//生产者函数 条件变量
void producerWithCondition(){
    for(int i = 0; i < 20; i++){
        lock->Acquire();
        while(list->NumInList() == 10){
            c_full->Wait(lock);
        }
        Product* pro = new Product;
        pro->value = i;
        list->Append(pro);
        printf("thread %s produce : %d. buffer:%d/10\n",currentThread->getName(),i,list->NumInList());
        c_empty->Signal(lock);
        lock->Release();
    }
}
//消费者函数 信号量
void consumerWithCondition(){
    for(int i = 0; i < 20; i++){
        lock->Acquire();
        while(list->NumInList() == 0){
            c_empty->Wait(lock);
        }
        Product* pro = (Product*)list->Remove();
        printf("thread %s consume : %d. buffer:%d/10\n",currentThread->getName(),pro->value,list->NumInList());
        delete pro;
        c_full->Signal(lock);
        lock->Release();
    }
}
//条件变量解决生产者消费者问题
void ThreadTest5(){
    lock = new Lock("lock");
    c_empty = new Condition("empty");
    c_full = new Condition("full");
    list= new List;
    Thread* pro1 = new Thread("producer");
    pro1->Fork(producerWithCondition,(void*)1);
    Thread* con = new Thread("consumer");
    con->Fork(consumerWithCondition,(void*)1);

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
    case 4:
        ThreadTest4();
        break;
    case 5:
        ThreadTest5();
        break;
    default:
	    printf("No test specified.\n");
	    break;
    }
}

