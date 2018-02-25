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

#define  check(value,info) { \
    if (!(value)){ std::cout<<info<<" error!\n";  \
                   _result=false; if (!_isInClear){ goto clear; } } }

bool ApkNormalized(const char* srcApk,const char* dstApk){
    bool _result=true;
    bool _isInClear=false;
    UnZipper unzipper;
    Zipper   zipper;
    UnZipper_init(&unzipper);
    Zipper_init(&zipper);
    
    check(UnZipper_openRead(&unzipper,srcApk),"UnZipper_openRead \""<<srcApk<<"\"");
    check(Zipper_openWrite(&zipper,dstApk),"Zipper_openWrite \""<<dstApk<<"\"");
    
    
    
    
clear:
    _isInClear=true;
    check(UnZipper_close(&unzipper),"UnZipper_close \""<<srcApk<<"\"");
    check(Zipper_close(&zipper),"Zipper_close \""<<dstApk<<"\"");
    return _result;
}
