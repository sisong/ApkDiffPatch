//  parallel.h
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

#ifndef ZipPatch_parallel_h
#define ZipPatch_parallel_h

//支持的2种线程库的实现;
//#define IS_USED_PTHREAD       1
//#define IS_USED_CPP11THREAD   1

#if ((IS_USED_PTHREAD) || (IS_USED_CPP11THREAD))
#   define IS_USED_MULTITHREAD  1
#else
#   define IS_USED_SINGLETHREAD 1
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
    //并行临界区锁;
    typedef void*   HLocker;
    HLocker     locker_new(void);
    void        locker_delete(HLocker locker);
    void        locker_enter(HLocker locker);
    void        locker_leave(HLocker locker);
    
    //同步变量;
    typedef void*   HCondvar;
#if (IS_USED_CPP11THREAD)
#   define TLockerBox void  /* std::unique_lock<std::mutex> */
#else
    struct TLockerBox {
        HLocker locker;
    };
#endif
    HCondvar    condvar_new(void);
    void        condvar_delete(HCondvar cond);
    void        condvar_wait(HCondvar cond,TLockerBox* lockerBox);
    void        condvar_signal(HCondvar cond);
    void        condvar_broadcast(HCondvar cond);
    
    //并行工作线程;
    typedef void (*TThreadRunCallBackProc)(int threadIndex,void* workData);
    void  thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                          int isUseThisThread,int threadIndexStart=0);
    
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <stddef.h> //for size_t ptrdiff_t

#if (IS_USED_PTHREAD)
    struct CAutoLocker:public TLockerBox {
        inline CAutoLocker(HLocker _locker){ locker=_locker; locker_enter(locker); }
        inline ~CAutoLocker(){ locker_leave(locker); }
    };
#endif
#if (IS_USED_CPP11THREAD)
#   include <thread>
    struct CAutoLocker:public std::unique_lock<std::mutex> {
        inline CAutoLocker(HLocker _locker)
            :std::unique_lock<std::mutex>(*(std::mutex*)_locker){ }
        inline ~CAutoLocker(){  }
    };
#endif

#if (IS_USED_MULTITHREAD)
    //通道交互数据;
    typedef void* TChanData;

    class _CChannel_import;
    //通道;
    class CChannel{
    public:
        explicit CChannel(ptrdiff_t maxDataCount=-1);
        ~CChannel();
        void close();
        bool is_can_fast_send(bool isWait); //mybe not need wait when send
        bool send(TChanData data,bool isWait); //data cannot 0
        TChanData accept(bool isWait); //threadID can 0;
    private:
        _CChannel_import* _import;
    };
#endif //IS_USED_MULTITHREAD

#endif //__cplusplus
#endif //ZipPatch_parallel_h
