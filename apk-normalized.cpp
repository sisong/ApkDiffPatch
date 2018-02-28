//  apk-normalized.cpp
//  ApkNormalized
//
//  Created by housisong on 2018/2/25.
//  Copyright © 2018年 housisong. All rights reserved.
//

//diff前对apk包进行规范化处理:
//  输入apk文件(v1版签名),输出重新按固定压缩算法生成的新apk文件(v1签名会被保留,v2签名会被删除)
//  (需要另外用Android工具对输出的apk文件执行v2签名)
#include <iostream>
#include "src/patch/Zipper.h"
#include "src/patch/membuf.h"

//  #include <time.h>
//  static double clock_s(){ return clock()*(1.0/CLOCKS_PER_SEC); }
#ifdef _WIN32
#include <windows.h>
static double clock_s(){ return GetTickCount()/1000.0; }
#else
//Unix-like system
#include <sys/time.h>
static double clock_s(){
    struct timeval t={0,0};
    int ret=gettimeofday(&t,0);
    assert(ret==0);
    return t.tv_sec + t.tv_usec/1000000.0;
}
#endif

bool ApkNormalized(const char* srcApk,const char* dstApk);

int main(int argc, const char * argv[]) {
    if (argc!=3){
        std::cout << "input: srcApk dstApk\n";
        return 1;
    }
    int exitCode=0;
    const char* srcApk=argv[1];
    const char* dstApk=argv[2];
    double time0=clock_s();
    if (!ApkNormalized(srcApk,dstApk)){
        std::cout << "ApkNormalized error\n";
        exitCode=1;
    }
    double time1=clock_s();
    std::cout<<"ApkNormalized  time:"<<(time1-time0)<<" s\n";
    return exitCode;
}

#define  check(value) { \
    if (!(value)){ std::cout<<#value<<" ERROR!\n";  \
        result=false; if (!_isInClear){ goto clear; } } }

bool ApkNormalized(const char* srcApk,const char* dstApk){
    bool result=true;
    bool _isInClear=false;
    int fileCount=0;
    MemBuf   memBuf;
    UnZipper unzipper;
    Zipper   zipper;
    UnZipper_init(&unzipper);
    Zipper_init(&zipper);
    
    check(UnZipper_openRead(&unzipper,srcApk));
    fileCount=UnZipper_fileCount(&unzipper);
    check(Zipper_openWrite(&zipper,dstApk,fileCount));
    
    for (int i=0; i<fileCount; ++i) {
        if (UnZipper_file_isCompressed(&unzipper,i)) {
            ZipFilePos_t uncompressedSize=UnZipper_file_uncompressedSize(&unzipper,i);
            assert(uncompressedSize==(size_t)uncompressedSize);
            memBuf.setNeedCap((size_t)uncompressedSize);
            MemBufWriter MemBufWriter(memBuf.buf(),uncompressedSize);
            check(UnZipper_fileData_decompressTo(&unzipper,i,memBuf.buf(),uncompressedSize));
            check(Zipper_file_appendWith(&zipper,&unzipper,i,memBuf.buf(),uncompressedSize,0));
        }else{
            check(Zipper_file_append(&zipper,&unzipper,i));
        }
    }
    for (int i=0; i<fileCount; ++i) {
        check(Zipper_fileHeader_append(&zipper,&unzipper,i));
    }
    check(Zipper_endCentralDirectory_append(&zipper,&unzipper));
clear:
    _isInClear=true;
    check(UnZipper_close(&unzipper));
    check(Zipper_close(&zipper));
    return result;
}
