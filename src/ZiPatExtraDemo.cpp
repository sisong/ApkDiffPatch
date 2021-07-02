//  ZiPatExtraDemo.cpp
//  ZiPatExtraDemo
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
#include <stdio.h>
#include <stdlib.h>
#include "../HDiffPatch/file_for_patch.h"
#include "patch/patch_types.h"
#include "patch/ZipDiffData.h"
#include "diff/DiffData.h"
#include "../HDiffPatch/libHDiffPatch/HDiff/private_diff/mem_buf.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif

// ZiPatExtraDemo: insert your private data to ZiPatFile(ZipDiff result), support ApkV2Sign ;
//   data will be write into NewZipFile(front of Central directory) when ZipPatch;

static bool addToExtra(const char* srcZiPatPath,const char* outZiPatPath,
                       const TByte* appendData,size_t dataLen);

#define _kRET_ERROR 1
int extra_cmd_line(int argc, const char * argv[]);

#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
        return _kRET_ERROR;
    SetDefaultStringLocale();
    return extra_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc,char* argv[]){
    return  extra_cmd_line(argc,(const char**)argv);
}
#   endif
#endif

int extra_cmd_line(int argc, const char * argv[]) {
    const char* srcZiPatPath=0;
    const char* outZiPatPath=0;
    if (argc!=4){
        printf("parameter: ZipDiffResult_ZiPatFileName  outDemo_ZiPatFileName appendData\n");
        return _kRET_ERROR;
    }
    srcZiPatPath =argv[1];
    outZiPatPath =argv[2];
    const char* appendData=argv[3];//NOTE: modify this for your require
    printf(" src ZiPat :\"%s\"\n",srcZiPatPath);
    printf(" out ZiPat :\"%s\"\n",outZiPatPath);
    printf("test append:\"%s\"\n",appendData);
    if (!(addToExtra(srcZiPatPath,outZiPatPath,(const TByte*)appendData,strlen(appendData)))){
        return _kRET_ERROR;
    }
    return 0;
}


#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }

static bool stream_copy(hpatch_TStreamOutput* dst,hpatch_StreamPos_t dstPos,
                        hpatch_TStreamInput* src,hpatch_StreamPos_t srcPos,hpatch_StreamPos_t len,
                        TByte* buf,size_t bufSize){
    bool        result=true;
    bool        _isInClear=false;
    hpatch_StreamPos_t endPos=dstPos+len;
    while (dstPos<endPos) {
        size_t clen=bufSize;
        if (clen>(endPos-dstPos)) clen=(size_t)(endPos-dstPos);
        check(src->read(src,srcPos,buf,buf+clen));
        check(dst->write(dst,dstPos,buf,buf+clen));
        srcPos+=clen;
        dstPos+=clen;
    }
clear:
    _isInClear=true;
    return result;
}

inline static void writeUInt64_to(unsigned char* out_buf8,hpatch_StreamPos_t v){
    writeUInt32_to(out_buf8,(uint32_t)v);
    writeUInt32_to(out_buf8+4,(uint32_t)(v>>32));
}

#define isExtraEdit(input) (0==memcmp(kExtraEdit,input,kExtraEditLen))

static bool addToExtra(const char* srcZiPatPath,const char* outZiPatPath,
                       const TByte* appendData,size_t dataLen){
    TByte*                      buf=0;
    const size_t                bufSize=64*1024;
    hpatch_TFileStreamInput     zipat;
    hpatch_TFileStreamOutput    output_file;
    hpatch_StreamPos_t          out_file_length;
    hpatch_StreamPos_t          editExtraPos=0;
    uint32_t                    oldExtraSize=0;
    uint32_t                    newExtraSize=0;
    ZipFilePos_t                oldV2BlockSize=0;
    bool                        isV2SignExtra=false;
    bool        result=true;
    bool        _isInClear=false;
    hpatch_TFileStreamInput_init(&zipat);
    hpatch_TFileStreamOutput_init(&output_file);
    
    buf=(TByte*)malloc(bufSize);
    check(buf!=0);
    
    check(hpatch_TFileStreamInput_open(&zipat,srcZiPatPath));
    
    check(zipat.base.streamSize>=4+kExtraEditLen);
    {//find Extra
        unsigned char buf4s[4+kExtraEditLen];
        const unsigned char * srcZiPatTag=buf4s+4;
        check(zipat.base.read(&zipat.base,zipat.base.streamSize-4-kExtraEditLen,buf4s,buf4s+4+kExtraEditLen));
        check(isExtraEdit(srcZiPatTag));//check tag
        oldExtraSize=readUInt32(buf4s);
        
        check(4+kExtraEditLen+oldExtraSize<zipat.base.streamSize);
        editExtraPos=zipat.base.streamSize-oldExtraSize-4-kExtraEditLen;
    }
    
    newExtraSize=oldExtraSize+(uint32_t)dataLen;
    out_file_length=zipat.base.streamSize+dataLen;
    check(hpatch_TFileStreamOutput_open(&output_file,outZiPatPath,out_file_length));
    //copy front
    check(stream_copy(&output_file.base,0,&zipat.base,0,editExtraPos,buf,bufSize));
    
    {//have Apk V2 Sign?
        ZipFilePos_t                v2sign_topPos=0;
        check(UnZipper_searchApkV2Sign(&zipat.base,editExtraPos+oldExtraSize,&v2sign_topPos,&oldV2BlockSize));
        isV2SignExtra=(v2sign_topPos==editExtraPos)&&(v2sign_topPos<editExtraPos+oldExtraSize);
        if (isV2SignExtra) check(8+oldV2BlockSize==oldExtraSize);
    }
    if (!isV2SignExtra){//insert data
        //insert example
        check(output_file.base.write(&output_file.base,editExtraPos,appendData,appendData+dataLen));
        //old Extra
        check(stream_copy(&output_file.base,editExtraPos+dataLen,&zipat.base,editExtraPos,oldExtraSize,buf,bufSize));
    }else{ //insert data to Apk V2 Sign
        const size_t APKSigningTagLen=16;
        hpatch_StreamPos_t oldV2BlockDataSize=oldV2BlockSize-8-APKSigningTagLen;
        hpatch_StreamPos_t writePos=editExtraPos;
        writeUInt64_to(buf,oldV2BlockSize+dataLen);
        check(output_file.base.write(&output_file.base,writePos,buf,buf+8)); //update high BlockSize
        writePos+=8;
        //copy Apk V2 Sign Block data
        check(stream_copy(&output_file.base,writePos,&zipat.base,editExtraPos+8,oldV2BlockDataSize,buf,bufSize));
        writePos+=oldV2BlockDataSize;
        //insert example   NOTE: You may have your own method of insert
        check(output_file.base.write(&output_file.base,writePos,appendData,appendData+dataLen));
        writePos+=dataLen;
        writeUInt64_to(buf,oldV2BlockSize+dataLen);
        check(output_file.base.write(&output_file.base,writePos,buf,buf+8));//update low BlockSize
        writePos+=8;
        //copy Apk V2 Sign tag
        check(stream_copy(&output_file.base,writePos,&zipat.base,editExtraPos+8+oldV2BlockDataSize+8,
                          APKSigningTagLen,buf,bufSize));
        writePos+=APKSigningTagLen;
        check(writePos-editExtraPos==newExtraSize);
    }

    {//write end
        writeUInt32_to(buf,newExtraSize);
        check(output_file.base.write(&output_file.base,editExtraPos+newExtraSize,buf,buf+4));
        check(output_file.base.write(&output_file.base,editExtraPos+newExtraSize+4,
                                     (const TByte*)kExtraEdit,(const TByte*)kExtraEdit+kExtraEditLen));
    }
clear:
    _isInClear=true;
    check(hpatch_TFileStreamOutput_close(&output_file));
    check(hpatch_TFileStreamInput_close(&zipat));
    if (buf) free(buf);
    return result;
}
