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
#include "src/patch/UnZipper.h"

void ApkNormalized(const char* srcApk,const char* dstApk);

int main(int argc, const char * argv[]) {
    if (argc!=3){
        std::cout << "input: srcApk dstApk\n";
        return 1;
    }
    const char* srcApk=argv[1];
    const char* dstApk=argv[2];
    std::cout << "Hello, ApkNormalized!\n";
    ApkNormalized(srcApk,dstApk);
    return 0;
}

void ApkNormalized(const char* srcApk,const char* dstApk){
    UnZipper unzipper(srcApk);
    Zipper zipper(dstApk);
    
}
