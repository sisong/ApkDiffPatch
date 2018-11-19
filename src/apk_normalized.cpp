//  apk_normalized.cpp
//  ApkNormalized
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

//apk_normalized是为了diff/patch过程兼容apk的v2版签名而提供;
//该过程对zip\jar\apk包进行规范化处理:
//   输入包文件,重新按固定压缩算法生成对齐的新包文件(扩展字段、注释、jar的签名和apk文件的v1签名会被保留,apk的v2签名数据会被删除)
//   规范化后可以用Android签名工具对输出的apk文件执行v2签名,比如:
//  apksigner sign --v1-signing-enabled true --v2-signing-enabled true --ks *.keystore --ks-pass pass:* --in normalized.apk --out result.apk

#include <iostream>
#include "normalized/normalized.h"
#include "../HDiffPatch/_clock_for_demo.h"
#include "diff/DiffData.h"

int main(int argc, const char * argv[]) {
    if (argc!=3){
        std::cout << "usage: ApkNormalized srcApk out_newApk\n"
                     "options:\n"
                     "  input srcApk file can *.zip *.jar *.apk file type;\n"
                     "  ApkNormalized normalized zip file: recompress all file data, \n"
                     "    align file data offset in zip file (compatible with AndroidSDK#zipalign),\n"
                     "    remove all data descriptor, reserve & normalized Extra field and Comment,\n"
                     "    compatible with jar sign(apk v1 sign), etc...\n"
                     "  if apk file use apk v2 sign,must Released Apk:=AndroidSDK#apksigner(out_newApk)\n"
                     "    after ApkNormalized;\n";
        return 1;
    }
    const char* srcApk=argv[1];
    const char* dstApk=argv[2];
    std::cout<<"src: \"" <<srcApk<< "\"\nout: \""<<dstApk<<"\"\n";
    double time0=clock_s();
    if (!ZipNormalized(srcApk,dstApk,kDefaultZipAlignSize,kDefaultZlibCompressLevel)){
        std::cout << "\nrun ApkNormalized ERROR!\n";
        return 1;
    }
    std::cout << "run ApkNormalized ok!\n";
    
    //check
    if (!getZipIsSame(srcApk,dstApk)){
        std::cout << "ApkNormalized result file check ERROR!\n";
        return 1;
    }
    std::cout<<"  check ApkNormalized result ok!\n";
    
    double time1=clock_s();
    std::cout<<"ApkNormalized time: "<<(time1-time0)<<" s\n";
    
    return 0;
}

