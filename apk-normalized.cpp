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

bool ApkNormalized(const char* srcApk,const char* dstApk);

int main(int argc, const char * argv[]) {
    if (argc!=3){
        std::cout << "input: srcApk dstApk\n";
        return 1;
    }
    const char* srcApk=argv[1];
    const char* dstApk=argv[2];
    std::cout << "Hello, ApkNormalized!\n";
    if (!ApkNormalized(srcApk,dstApk))
        return 1;
    return 0;
}

#define  check(value) { \
    if (!(value)){ std::cout<<#value<<" ERROR!\n";  \
        _result=false; if (!_isInClear){ goto clear; } } }

bool ApkNormalized(const char* srcApk,const char* dstApk){
    bool _result=true;
    bool _isInClear=false;
    int fileCount=0;
    MemBuf   memBuf;
    UnZipper unzipper;
    Zipper   zipper;
    UnZipper_init(&unzipper);
    Zipper_init(&zipper);
    
    check(UnZipper_openRead(&unzipper,srcApk));
    fileCount=UnZipper_FileEntry_count(&unzipper);
    check(Zipper_openWrite(&zipper,dstApk,fileCount));
    
    for (int i=0; i<fileCount; ++i) {
        if (UnZipper_FileEntry_isCompressed(&unzipper,i)) {
            ZipFilePos_t uncompressedSize=UnZipper_FileEntry_uncompressedSize(&unzipper,i);
            assert(uncompressedSize==(size_t)uncompressedSize);
            memBuf.setNeedCap((size_t)uncompressedSize);
            check(UnZipper_FileEntry_decompress(&unzipper,i,memBuf.buf()));
            check(Zipper_FileEntry_appendWith(&zipper,&unzipper,i,memBuf.buf(),uncompressedSize,true));
        }else{
            check(Zipper_FileEntry_append(&zipper,&unzipper,i));
        }
    }
    for (int i=0; i<fileCount; ++i) {
        check(Zipper_FileHeader_append(&zipper,&unzipper,i));
    }
    check(Zipper_EndCentralDirectory_append(&zipper,&unzipper));
clear:
    _isInClear=true;
    check(UnZipper_close(&unzipper));
    check(Zipper_close(&zipper));
    return _result;
}
