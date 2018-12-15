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
#include <assert.h>


#if (IS_USED_PTHREAD)
#   ifdef WIN32
#       include "windows.h" //for Sleep
#       //pragma comment(lib,"pthread.lib") //for static pthread lib
#       //define PTW32_STATIC_LIB  //for static pthread lib
#   endif
#   include <pthread.h>
#   include <stdlib.h>
#endif
#if (IS_USED_CPP11THREAD)
#   include <thread>
#endif

#if (IS_USED_PTHREAD)
#include <string.h> //for memset
#include <stdexcept>
#include <string>
#include <stdio.h>

static std::string i2a(int ivalue){
    const int kMaxNumCharSize =32;
    std::string result;
    result.resize(kMaxNumCharSize);
    int len=sprintf(&result[0],"%d",ivalue);
    result.resize(len);
    return result;
}
#define _check_pthread(result,func_name) { \
    if (result!=0) throw std::runtime_error(func_name "() return "+i2a(result)+" error!"); }

HLocker locker_new(void){
    pthread_mutex_t* self=new pthread_mutex_t();
    int rt=pthread_mutex_init(self, 0);
    if (rt!=0){
        delete self;
        _check_pthread(rt,"pthread_mutex_init");
    }
    return  self;
}
void locker_delete(HLocker locker){
    if (locker!=0){
        pthread_mutex_t* self=(pthread_mutex_t*)locker;
        int rt=pthread_mutex_destroy(self);
        delete self;
        _check_pthread(rt,"pthread_mutex_destroy");
    }
}

void locker_enter(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    int rt=pthread_mutex_lock(self);
    _check_pthread(rt,"pthread_mutex_lock");
}
void locker_leave(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    int rt=pthread_mutex_unlock(self);
    _check_pthread(rt,"pthread_mutex_unlock");
}

HCondvar condvar_new(void){
    pthread_cond_t* self=new pthread_cond_t();
    int rt=pthread_cond_init(self,0);
    if (rt!=0){
        delete self;
        _check_pthread(rt,"pthread_cond_init");
    }
    return self;
}
void    condvar_delete(HCondvar cond){
    if (cond){
        pthread_cond_t* self=(pthread_cond_t*)cond;
        int rt=pthread_cond_destroy(self);
        delete self;
        _check_pthread(rt,"pthread_cond_destroy");
    }
}
void    condvar_wait(HCondvar cond,TLockerBox* lockerBox){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_wait(self,(pthread_mutex_t*)(lockerBox->locker));
    _check_pthread(rt,"pthread_cond_wait");
}
void    condvar_signal(HCondvar cond){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_signal(self);
    _check_pthread(rt,"pthread_cond_signal");
}
void    condvar_broadcast(HCondvar cond){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_broadcast(self);
    _check_pthread(rt,"pthread_cond_broadcast");
}

void this_thread_yield(){
#ifdef __APPLE__
    pthread_yield_np();
#else
#   ifdef WIN32
    Sleep(0);
#   else
    pthread_yield();
#   endif
#endif
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

void thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                     int isUseThisThread,int threadIndexStart){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexStart,workData);
        }else{
            _TPThreadData* pt=new _TPThreadData();
            pt->threadIndex=i+threadIndexStart;
            pt->threadProc=threadProc;
            pt->workData=workData;
            pthread_t t; memset(&t,0,sizeof(pthread_t));
            int rt=pthread_create(&t,0,_pt_threadProc,pt);
            if (rt!=0){
                delete pt;
                _check_pthread(rt,"pthread_create");
            }
            rt=pthread_detach(t);
            _check_pthread(rt,"pthread_detach");
        }
    }
}
#endif //IS_USED_PTHREAD

#if (IS_USED_CPP11THREAD)
HLocker locker_new(void){
    return new std::mutex();
}
void locker_delete(HLocker locker){
    if (locker!=0)
        delete (std::mutex*)locker;
}

void locker_enter(HLocker locker){
    std::mutex* self=(std::mutex*)locker;
    self->lock();
    
}
void locker_leave(HLocker locker){
    std::mutex* self=(std::mutex*)locker;
    self->unlock();
}

HCondvar condvar_new(void){
    std::condition_variable* self=new std::condition_variable();
    return self;
} 
void    condvar_delete(HCondvar cond){
    if (cond){
        std::condition_variable* self=(std::condition_variable*)cond;
        delete self;
    }
}
void    condvar_wait(HCondvar cond,TLockerBox* locker){
    std::condition_variable* self=(std::condition_variable*)cond;
    CAutoLocker* _locker=(CAutoLocker*)locker;
    self->wait(*_locker);
}
void    condvar_signal(HCondvar cond){
    std::condition_variable* self=(std::condition_variable*)cond;
    self->notify_one();
}
void    condvar_broadcast(HCondvar cond){
    std::condition_variable* self=(std::condition_variable*)cond;
    self->notify_all();
}

void this_thread_yield(){
    std::this_thread::yield();
}

void thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                     int isUseThisThread,int threadIndexStart){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexStart,workData);
        }else{
            std::thread t(threadProc,i+threadIndexStart,workData);
            t.detach();
        }
    }
}
#endif //IS_USED_CPP11THREAD

#if (IS_USED_MULTITHREAD)
#include <deque>

class _CChannel_import{
public:
    explicit _CChannel_import(ptrdiff_t maxDataCount)
    :_locker(0),_wantSendCond(0),_sendCond(0),_acceptCond(0),
    _maxDataCount(maxDataCount),_waitingCount(0),_isClosed(false){
        _locker=locker_new();
        _wantSendCond=condvar_new();
        _sendCond=condvar_new();
        _acceptCond=condvar_new();
    }
    ~_CChannel_import(){
        close();
        while (true) { //wait all thread exit
            if (_waitingCount==0) break;
            {
                CAutoLocker locker(_locker);
                if (_waitingCount==0) break;
            }
            this_thread_yield(); //todo:优化?
        }
        locker_delete(_locker);
        assert(_dataList.empty()); // error, why?
    }
    void close(){
        if (_isClosed) return;
        {
            CAutoLocker locker(_locker);
            _isClosed=true;
        }
        condvar_broadcast(_wantSendCond);
        condvar_broadcast(_sendCond);
        condvar_broadcast(_acceptCond);
    }
    bool is_can_fast_send(bool isWait){
        if (_maxDataCount<0) return true;
        if (_maxDataCount==0) return false;
        while (true) {
            if (_isClosed) return false;
            {
                CAutoLocker locker(_locker);
                if (_isClosed) return false;
                if (_dataList.size()<(size_t)_maxDataCount) {
                    return true;
                }else if(!isWait){
                    return false;
                }//else wait
                ++_waitingCount;
                condvar_wait(_wantSendCond,&locker);
                --_waitingCount;
            }
        }
    }
    
    bool send(TChanData data,bool isWait){
        assert(data!=0);
        while (true) {
            if (_isClosed) return false;
            {
                CAutoLocker locker(_locker);
                if (_isClosed) return false;
                if ((_maxDataCount<=0)||(_dataList.size()<(size_t)_maxDataCount)) {
                    _dataList.push_back(data); break; //ok
                }else if(!isWait){
                    return false;
                }//else wait
                ++_waitingCount;
                condvar_wait(_sendCond,&locker);
                --_waitingCount;
            }
        }
        condvar_signal(_acceptCond);
        if (_maxDataCount==0){ //must wait
            while (true) { //wait _dataList empty
                if (_isClosed) break;
                {
                    CAutoLocker locker(_locker);
                    if (_isClosed) break;
                    if (_dataList.empty()) break;
                }
                this_thread_yield(); //todo:优化;
            }
        }
        return true;
    }
    TChanData accept(bool isWait){
        TChanData result=0;
        while (true) {
            CAutoLocker locker(_locker);
            if (!_dataList.empty()) {
                result=_dataList.front();
                _dataList.pop_front();
                break; //ok
            }else if(_isClosed){
                return 0;
            }else if(!isWait){
                return 0;
            }//else wait
            ++_waitingCount;
            condvar_wait(_acceptCond,&locker);
            --_waitingCount;
        }
        condvar_signal(_sendCond);
        condvar_signal(_wantSendCond);
        return result;
    }
private:
    HLocker     _locker;
    HCondvar    _wantSendCond;
    HCondvar    _sendCond;
    HCondvar    _acceptCond;
    std::deque<TChanData>   _dataList;
    const ptrdiff_t         _maxDataCount;
    volatile size_t         _waitingCount;
    volatile bool           _isClosed;
};


CChannel::CChannel(ptrdiff_t maxDataCount):_import(0){
    _import=new _CChannel_import(maxDataCount);
}
CChannel::~CChannel(){
    delete _import;
}
void CChannel::close(){
    _import->close();
}
bool CChannel::is_can_fast_send(bool isWait){
    return _import->is_can_fast_send(isWait);
}
bool CChannel::send(TChanData data,bool isWait){
    return _import->send(data,isWait);
}
TChanData CChannel::accept(bool isWait){
    return _import->accept(isWait);
}

#endif
