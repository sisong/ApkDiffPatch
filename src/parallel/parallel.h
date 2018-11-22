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

#if ((IS_USED_PTHREAD) | (IS_USED_CPP11THREAD))
#   define IS_USED_MULTITHREAD  1
#   define IS_USED_SINGLETHREAD 0
#else
#   define IS_USED_MULTITHREAD  0
#   define IS_USED_SINGLETHREAD 1
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
    //并行临界区;
    typedef void*   HLocker;
    HLocker     locker_new(void);
    void        locker_delete(HLocker locker);
    void        locker_enter(HLocker locker);
    void        locker_leave(HLocker locker);
    
    //并行工作线程;
    typedef void (*TThreadRunCallBackProc)(int threadIndex,void* workData);
    bool  thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,int isUseThisThread);
    
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

struct CAutoLoker {
    inline CAutoLoker(HLocker locker):m_locker(locker) { locker_enter(locker); }
    inline ~CAutoLoker(){ locker_leave(m_locker); }
private:
    HLocker m_locker;
};

#endif
#endif //ZipPatch_parallel_h
