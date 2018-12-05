//  parallel.cpp
//  ZipPatch
/*
 The MIT License (MIT)
 Copyright (c) 2018 HouSisong
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */
#include "parallel.h"


#if (IS_USED_PTHREAD)
#   include <pthread.h>
#   include <stdlib.h>
#endif
#if (IS_USED_CPP11THREAD)
#    include <thread>
#    include <atomic>
#endif

#if (IS_USED_SINGLETHREAD)
HLocker locker_new(void){
    return (HLocker)(1);
}
void    locker_delete(HLocker locker){
    assert(locker==(HLocker)(1));
}
void    locker_loop_enter(HLocker locker){
    //nothing
}
void    locker_leave(HLocker locker){
    //nothing
}

void this_thread_yield(){
    //nothing
}

bool thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,int isUseThisThread){
    for (int i=0; i<threadCount; ++i) {
        threadProc(i,workData);
    }
    return true;
}
#endif //IS_USED_SINGLETHREAD


#if (IS_USED_PTHREAD)
HLocker locker_new(void){
    pthread_mutex_t* self=new pthread_mutex_t();
    pthread_mutex_init(self, 0);
    return  self;
}
void locker_delete(HLocker locker){
    if (locker!=0){
        pthread_mutex_t* self=(pthread_mutex_t*)locker;
        pthread_mutex_destroy(self);
        delete self;
    }
}

static void _pthread_mutex_loopLock(pthread_mutex_t* locker){
    while (true) {
        if (0==pthread_mutex_trylock(locker))
            break;
        else
            this_thread_yield();
    }
}
void locker_loop_enter(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    _pthread_mutex_loopLock(self);
}
void locker_leave(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    pthread_mutex_unlock(self);
}

void this_thread_yield(){
    pthread_yield_np();
}

struct _TPThreadData{
    TThreadRunCallBackProc  threadProc;
    int                     threadIndex;
    void*                   workData;
};
void* _pt_threadProc(void* _pt){
    _TPThreadData* pt=(_TPThreadData*)_pt;
    pt->threadProc(pt->threadIndex,pt->workData);
    delete pt;
    return 0;
}

bool thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,int isUseThisThread){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i,workData);
        }else{
            _TPThreadData* pt=new _TPThreadData();
            pt->threadIndex=i;
            pt->threadProc=threadProc;
            pt->workData=workData;
            pthread_t t=0;
            if (pthread_create(&t,0,_pt_threadProc,pt)!=0) return false;
            if (pthread_detach(t)!=0) return false;
        }
    }
    return true;
}
#endif //IS_USED_PTHREAD

#if (IS_USED_CPP11THREAD)
HLocker locker_new(void){
    return new std::atomic<int>(0);
}
void locker_delete(HLocker locker){
    if (locker!=0)
        delete (std::atomic<int>*)locker;
}

void locker_loop_enter(HLocker locker){
    std::atomic<int>& self=*(std::atomic<int>*)locker;
    while (true) {
        int v=++self;
        if (v==1)
            break;
        --self;
        this_thread_yield();
    }
}
void locker_leave(HLocker locker){
    std::atomic<int>& self=*(std::atomic<int>*)locker;
    --self;
}

void this_thread_yield(){
    std::this_thread::yield();
}

bool thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,int isUseThisThread){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i,workData);
        }else{
            try{
                std::thread t(threadProc,i,workData);
                t.detach();
            }catch(...){
                return false;
            }
        }
    }
    return true;
}
#endif //IS_USED_CPP11THREAD
