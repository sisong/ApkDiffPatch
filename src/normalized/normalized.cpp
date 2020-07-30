//  normalized.cpp
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
#include "normalized.h"
#include <vector>
#include <string>
#include "../patch/Zipper.h"
#include "../diff/DiffData.h"

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }

struct TFileValue{
    std::string fileName;
    int         fileIndex;
    struct TCmp{
        inline TCmp(int fileCount):_fileCount(fileCount){}
        static bool inline isInSignDir(const std::string& s){
            return (s.find("META-INF/")==0)||(s.find("META-INF\\")==0);
        }
        static bool inline isEndWith(const std::string& s,const char* sub){
            return (s.find(sub)==s.size()-strlen(sub));
        }
        size_t _v(const TFileValue& x)const{
            size_t xi=x.fileIndex;
            if (isInSignDir(x.fileName)){
                xi+=_fileCount;
                     if (isEndWith(x.fileName,".SF"))  xi+=_fileCount;
                else if (isEndWith(x.fileName,".RSA")) xi+=_fileCount*2;
                else if (isEndWith(x.fileName,".MF"))  xi+=_fileCount*3;
            }
            return  xi;
        }
        inline bool operator ()(const TFileValue& x,const TFileValue& y)const{
            return _v(x)<_v(y);
        }
        int _fileCount;
    };
};
static void getFiles(const UnZipper* zip,std::vector<TFileValue>& out_files){
    int fileCount=UnZipper_fileCount(zip);
    for (int i=0; i<fileCount; ++i) {
        const char* fn=UnZipper_file_nameBegin(zip,i);
        TFileValue fi;
        fi.fileIndex=i;
        fi.fileName.assign(std::string(fn,fn+UnZipper_file_nameLen(zip,i)));
        out_files.push_back(fi);
    }
}

bool ZipNormalized(const char* srcApk,const char* dstApk,int ZipAlignSize,int compressLevel){
    bool result=true;
    bool _isInClear=false;
    int  fileCount=0;
    bool isHaveApkV2Sign=false;
    int  jarSignFileCount=0;
    std::vector<int>   fileIndexs;
    UnZipper unzipper;
    Zipper   zipper;
    UnZipper_init(&unzipper);
    Zipper_init(&zipper);
    
    check(UnZipper_openFile(&unzipper,srcApk));
    fileCount=UnZipper_fileCount(&unzipper);
    check(Zipper_openFile(&zipper,dstApk,fileCount,ZipAlignSize,compressLevel,kDefaultZlibCompressMemLevel));
    
    {//sort file
        std::vector<TFileValue> files;
        getFiles(&unzipper,files);
        std::sort(files.begin(),files.end(),TFileValue::TCmp(fileCount));
        for (int i=0; i<fileCount; ++i) {
            fileIndexs.push_back(files[i].fileIndex);
        }
    }
    for (int i=0; i<fileCount; ++i) {
        if (UnZipper_file_isApkV1_or_jarSign(&unzipper,i))
            ++jarSignFileCount;
    }
    isHaveApkV2Sign=UnZipper_isHaveApkV2Sign(&unzipper);
    assert((size_t)fileCount==fileIndexs.size());
    
    printf("\n");
    for (int i=0; i<(int)fileIndexs.size(); ++i) {
        int fileIndex=fileIndexs[i];
        std::string fileName=zipFile_name(&unzipper,fileIndex);
        printf("\"%s\"\n",fileName.c_str());
        bool isAlwaysReCompress=true;
        check(Zipper_file_append_copy(&zipper,&unzipper,fileIndex,isAlwaysReCompress));
    }
    printf("\n");

    //no run: check(Zipper_copyExtra_before_fileHeader(&zipper,&unzipper));
    for (int i=0; i<(int)fileIndexs.size(); ++i) {
        int fileIndex=fileIndexs[i];
        check(Zipper_fileHeader_append(&zipper,&unzipper,fileIndex));
    }
    check(Zipper_endCentralDirectory_append(&zipper,&unzipper));
    
    if (jarSignFileCount>0)
        printf("NOTE: src found JarSign(ApkV1Sign) (%d file)\n",jarSignFileCount);
    if (isHaveApkV2Sign)
        printf("WARNING: src found ApkV2Sign and not save to dst(%d Byte, need re sign)\n",(int)UnZipper_ApkV2SignSize(&unzipper));
    printf("src fileCount:%d\nout fileCount:%d\n\n",fileCount,(int)fileIndexs.size());

clear:
    _isInClear=true;
    check(UnZipper_close(&unzipper));
    check(Zipper_close(&zipper));
    return result;
}
