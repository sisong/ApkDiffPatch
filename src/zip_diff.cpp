//  zip_diff.cpp
//  ZipDiff
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diff/Differ.h"
#include "diff/DiffData.h"
#include "patch/patch_types.h"
#include "../HDiffPatch/_clock_for_demo.h"
#include "../HDiffPatch/_atosize.h"
#include "../HDiffPatch/libHDiffPatch/HDiff/private_diff/mem_buf.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif

#define _CompressPlugin_lzma //default use lzma
#define _CompressPlugin_lzma2

static void printUsage(){
    printf("diff usage: ZipDiff oldZipFile newZipFile outDiffFileName [-c-...] [-m-matchScore] [-v]\n"
           "test usage: ZipDiff -t oldZipFile newZipFile testDiffFileName\n"
           "options:\n"
           "  input oldZipFile & newZipFile file can *.zip *.jar *.apk file type;\n"
           "  -c-compressType[-compressLevel]\n"
           "      set outDiffFile Compress type & level, DEFAULT "
#ifdef _CompressPlugin_lzma
           "-c-lzma-7-4m\n"
#else
           "-c-zlib-9\n"
#endif
           "      support compress type & level:\n"
           "        -c-zlib[-{1..9}]                DEFAULT level 9\n"
#ifdef _CompressPlugin_lzma
           "        -c-lzma[-{0..9}[-dictSize]]     DEFAULT level 7\n"
           "            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 4m\n"
#   if (_IS_USED_MULTITHREAD)
           "            support run by 2-thread parallel.\n"
#   endif
#endif
#ifdef _CompressPlugin_lzma2
           "        -c-lzma2[-{0..9}[-dictSize]]    DEFAULT level 7\n"
           "            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 4m\n"
#   if (_IS_USED_MULTITHREAD)
           "            support run by multi-thread parallel, fast!\n"
#   endif
           "            WARNING: code not compatible with it compressed by -c-lzma!\n"
#endif
           "  -m-matchScore\n"
           "      matchScore>=0, DEFAULT -m-3.\n"
           "  -d  Diff only, do't run patch check, DEFAULT run patch check.\n"
           "  -t  Test only, run patch check, ZipPatch(oldZipFile,testDiffFile)==newZipFile ? \n"
#if (_IS_USED_MULTITHREAD)
           "  -p-parallelThreadNumber\n"
           "      if parallelThreadNumber>1 then open multi-thread Parallel mode;\n"
           "      DEFAULT -p-4; requires more memory!\n"
#endif
           "  -v  output Version info. \n");
}

#ifdef _CompressPlugin_lzma
#include "../lzma/C/LzmaDec.h" // http://www.7-zip.org/sdk.html
#include "../lzma/C/LzmaEnc.h"
#endif
#ifdef _CompressPlugin_lzma2
#include "../lzma/C/LzmaDec.h"
#include "../lzma/C/LzmaEnc.h"
#include "../lzma/C/Lzma2Dec.h"
#include "../lzma/C/Lzma2Enc.h"
#include "../lzma/C/MtCoder.h" //for MTCODER__THREADS_MAX
#endif
#include "patch/patch_types.h"
#include "../HDiffPatch/compress_plugin_demo.h"
#include "../HDiffPatch/decompress_plugin_demo.h"

#define  kDefaultLzmaDictSize         (1<<22)

#define ZIPDIFF_OPTIONS_ERROR 1
#define _options_check(value,errorInfo){ \
    if (!(value)) { printf("options " errorInfo " ERROR!\n"); printUsage(); return ZIPDIFF_OPTIONS_ERROR; } }

int zipdiff_cmd_line(int argc, const char * argv[]);

#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
        return ZIPDIFF_OPTIONS_ERROR;
    SetDefaultStringLocale();
    return zipdiff_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc,char* argv[]){
    return  zipdiff_cmd_line(argc,(const char**)argv);
}
#   endif
#endif

static bool _tryGetCompressSet(const char** isMatchedType,const char* ptype,const char* ptypeEnd,
                               const char* cmpType,const char* cmpType2=0,
                               size_t* compressLevel=0,size_t levelMin=0,size_t levelMax=0,size_t levelDefault=0,
                               size_t* dictSize=0,size_t dictSizeMin=0,size_t dictSizeMax=0,size_t dictSizeDefault=0){
    if (*isMatchedType) return true; //ok
    const size_t ctypeLen=strlen(cmpType);
    const size_t ctype2Len=(cmpType2!=0)?strlen(cmpType2):0;
    if ( ((ctypeLen==(size_t)(ptypeEnd-ptype))&&(0==strncmp(ptype,cmpType,ctypeLen)))
        || ((cmpType2!=0)&&(ctype2Len==(size_t)(ptypeEnd-ptype))&&(0==strncmp(ptype,cmpType2,ctype2Len))) )
        *isMatchedType=cmpType; //ok
    else
        return true;//type mismatch
    
    if ((compressLevel)&&(ptypeEnd[0]=='-')){
        const char* plevel=ptypeEnd+1;
        const char* plevelEnd=findUntilEnd(plevel,'-');
        if (!a_to_size(plevel,plevelEnd-plevel,compressLevel)) return false; //error
        if (*compressLevel<levelMin) *compressLevel=levelMin;
        else if (*compressLevel>levelMax) *compressLevel=levelMax;
        if ((dictSize)&&(plevelEnd[0]=='-')){
            const char* pdictSize=plevelEnd+1;
            const char* pdictSizeEnd=findUntilEnd(pdictSize,'-');
            if (!kmg_to_size(pdictSize,pdictSizeEnd-pdictSize,dictSize)) return false; //error
            if (*dictSize<dictSizeMin) *dictSize=dictSizeMin;
            else if (*dictSize>dictSizeMax) *dictSize=dictSizeMax;
        }else{
            if (plevelEnd[0]!='\0') return false; //error
            if (dictSize) *dictSize=dictSizeDefault;
        }
    }else{
        if (ptypeEnd[0]!='\0') return false; //error
        if (compressLevel) *compressLevel=levelDefault;
        if (dictSize) *dictSize=dictSizeDefault;
    }
    return true;
}

static int _checkSetCompress(hdiff_TCompress** out_compressPlugin,
                             hpatch_TDecompress** out_decompressPlugin,
                             const char* ptype,const char* ptypeEnd){
    const char* isMatchedType=0;
    size_t      compressLevel=0;
#if (defined _CompressPlugin_lzma)||(defined _CompressPlugin_lzma2)
    size_t       dictSize=0;
    const size_t defaultDictSize=kDefaultLzmaDictSize;
#endif
#ifdef _CompressPlugin_zlib
    _options_check(_tryGetCompressSet(&isMatchedType,
                                      ptype,ptypeEnd,"zlib",0,&compressLevel,1,9,9),"-c-zlib-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"zlib"))){
         static TCompressPlugin_zlib _zlibCompressPlugin=zlibCompressPlugin;
         _zlibCompressPlugin.compress_level=(int)compressLevel;
         _zlibCompressPlugin.mem_level=kDefaultZlibCompressMemLevel;
         *out_compressPlugin=&_zlibCompressPlugin.base;
         *out_decompressPlugin=&zlibDecompressPlugin; }
#endif
#ifdef _CompressPlugin_lzma
    _options_check(_tryGetCompressSet(&isMatchedType,
                                      ptype,ptypeEnd,"lzma",0,&compressLevel,0,9,7, &dictSize,1<<12,
                                      (sizeof(size_t)<=4)?(1<<27):((size_t)3<<29),defaultDictSize),"-c-lzma-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"lzma"))){
         static TCompressPlugin_lzma _lzmaCompressPlugin=lzmaCompressPlugin;
         _lzmaCompressPlugin.compress_level=(int)compressLevel;
         _lzmaCompressPlugin.dict_size=(int)dictSize;
         *out_compressPlugin=&_lzmaCompressPlugin.base;
         *out_decompressPlugin=&lzmaDecompressPlugin;  }
#endif
#ifdef _CompressPlugin_lzma2
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"lzma2",0,
                                      &compressLevel,0,9,7, &dictSize,1<<12,
                                      (sizeof(size_t)<=4)?(1<<27):((size_t)3<<29),defaultDictSize),"-c-lzma2-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"lzma2"))){
         static TCompressPlugin_lzma2 _lzma2CompressPlugin=lzma2CompressPlugin;
         _lzma2CompressPlugin.compress_level=(int)compressLevel;
         _lzma2CompressPlugin.dict_size=(int)dictSize;
         *out_compressPlugin=&_lzma2CompressPlugin.base;
         *out_decompressPlugin=&lzma2DecompressPlugin;  }
#endif
    _options_check((*out_compressPlugin!=0)&&(*out_decompressPlugin!=0),"-c-?");
    return 0; //ok
}


#define _kNULL_VALUE    (-1)
#define _kNULL_SIZE     (~(size_t)0)

#define _THREAD_NUMBER_NULL     0
#define _THREAD_NUMBER_MIN      1
#define _THREAD_NUMBER_DEFUALT  kDefaultCompressThreadNumber
#define _THREAD_NUMBER_MAX      (1<<8)

int zipdiff_cmd_line(int argc, const char * argv[]) {
    const char* oldZipPath     =0;
    const char* newZipPath     =0;
    const char* outDiffFileName=0;
    hpatch_BOOL isDiff=_kNULL_VALUE;
    hpatch_BOOL isPatchCheck   = _kNULL_VALUE;
    hpatch_BOOL isOutputVersion= _kNULL_VALUE;
    size_t      diffMatchScore = _kNULL_SIZE;
    size_t      threadNum = _THREAD_NUMBER_NULL;
    hdiff_TCompress*        compressPlugin=0;
    hpatch_TDecompress*     decompressPlugin=0;
#define kMax_arg_values_size 3
    const char * arg_values[kMax_arg_values_size]={0};
    int          arg_values_size=0;
    int         i;
    for (i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check((op!=0)&&(strlen(op)>0),"?");
        if (op[0]!='-'){
            _options_check(arg_values_size<kMax_arg_values_size,"count");
            arg_values[arg_values_size]=op;
            ++arg_values_size;
            continue;
        }
        _options_check((op!=0)&&(op[0]=='-'),"?");
        switch (op[1]) {
            case 'm':{
                if (op[2]=='-'){
                    _options_check(diffMatchScore==_kNULL_SIZE,"-m-?")
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&diffMatchScore),"-m-?");
                    const size_t kMaxDiffMatchScore=1024;
                    _options_check((0<=diffMatchScore)&&(diffMatchScore<=kMaxDiffMatchScore),"-m-?");
                }else{
                    _options_check(hpatch_FALSE,"-m?");
                }
            } break;
#if (_IS_USED_MULTITHREAD)
            case 'p':{
                _options_check((threadNum==_THREAD_NUMBER_NULL)&&(op[2]=='-'),"-p-?");
                const char* pnum=op+3;
                _options_check(a_to_size(pnum,strlen(pnum),&threadNum),"-p-?");
                _options_check(threadNum>=_THREAD_NUMBER_MIN,"-p-?");
            } break;
#endif
            case 'c':{
                _options_check((compressPlugin==0)&&(op[2]=='-'),"-c");
                const char* ptype=op+3;
                const char* ptypeEnd=findUntilEnd(ptype,'-');
                int result=_checkSetCompress(&compressPlugin,&decompressPlugin,ptype,ptypeEnd);
                if (0!=result)
                    return result;
            } break;
            case 'v':{
                _options_check((isOutputVersion==_kNULL_VALUE)&&(op[2]=='\0'),"-v");
                isOutputVersion=hpatch_TRUE;
            } break;
            case 't':{
                _options_check((isPatchCheck==_kNULL_VALUE)&&(op[2]=='\0'),"-t");
                isPatchCheck=hpatch_TRUE; //test diffFile
            } break;
            case 'd':{
                _options_check((isDiff==_kNULL_VALUE)&&(op[2]=='\0'),"-d");
                isDiff=hpatch_TRUE; //diff only
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    if (compressPlugin==0){
#ifdef _CompressPlugin_lzma
        static TCompressPlugin_lzma myLzmaCompressPlugin=lzmaCompressPlugin;
        myLzmaCompressPlugin.compress_level=7;
        myLzmaCompressPlugin.dict_size=kDefaultLzmaDictSize;
        compressPlugin=&myLzmaCompressPlugin.base;
        decompressPlugin=&lzmaDecompressPlugin;
#else
        static TCompressPlugin_zlib myZlibCompressPlugin=zlibCompressPlugin;
        myZlibCompressPlugin.compress_level=9;
        myZlibCompressPlugin.mem_level=kDefaultZlibCompressMemLevel;
        compressPlugin=&myZlibCompressPlugin.base;
        decompressPlugin=&zlibDecompressPlugin;
#endif
    }
    if (threadNum==_THREAD_NUMBER_NULL)
        threadNum=_THREAD_NUMBER_DEFUALT;
    else if (threadNum>_THREAD_NUMBER_MAX)
        threadNum=_THREAD_NUMBER_MAX;
    if (compressPlugin!=0){
        compressPlugin->setParallelThreadNumber(compressPlugin,(int)threadNum);
    }
    
    if (diffMatchScore==_kNULL_SIZE)
        diffMatchScore=kDefaultDiffMatchScore;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isOutputVersion){
        printf("ApkDiffPatch::ZipDiff v" APKDIFFPATCH_VERSION_STRING "\n\n");
        if (arg_values_size==0)
            return 0; //ok
    }
    
    if ((isDiff==hpatch_TRUE)&&(isPatchCheck==_kNULL_VALUE))
        isPatchCheck=hpatch_FALSE;
    if ((isDiff==_kNULL_VALUE)&&(isPatchCheck==hpatch_TRUE))
        isDiff=hpatch_FALSE;
    if (isDiff==_kNULL_VALUE)
        isDiff=hpatch_TRUE;
    if (isPatchCheck==_kNULL_VALUE)
        isPatchCheck=hpatch_TRUE;
    assert(isPatchCheck||isDiff);

    _options_check(arg_values_size==3,"count");
    
    oldZipPath      =arg_values[0];
    newZipPath      =arg_values[1];
    outDiffFileName =arg_values[2];
    printf((isDiff?"oldZip  :\"%s\"\nnewZip  :\"%s\"\noutDiff :\"%s\"\n":
                   "oldZip  :\"%s\"\nnewZip  :\"%s\"\ntestDiff:\"%s\"\n"),
                    oldZipPath,newZipPath,outDiffFileName);

    double time0=clock_s();
    bool   isNewZipApkV2SignNoError=true;
    if (isDiff){
        if (!ZipDiff(oldZipPath,newZipPath,outDiffFileName,compressPlugin,decompressPlugin,
                     (int)diffMatchScore,&isNewZipApkV2SignNoError)){
            printf("ZipDiff error!\n");
            return 1;
        }//else
        printf("ZipDiff time: %.3f s\n",(clock_s()-time0));
    }
    
    int exitCode=0;
    if (isPatchCheck){
        printf("\nrun ZipPatch:\n");
        for (int threadNum=1;threadNum<=4;threadNum*=4){
#if (_IS_USED_MULTITHREAD)
#else
            if (threadNum>1) break;
#endif
            double time2=clock_s();
            TCheckZipDiffResult rt=checkZipDiff(oldZipPath,newZipPath,outDiffFileName,threadNum);
            exitCode=((rt==CHECK_BYTE_BY_BYTE_EQUAL_TRUE)||(rt==CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_FALSE))?0:1;
            switch (rt) {
                case CHECK_BYTE_BY_BYTE_EQUAL_TRUE:{
                    printf("  check ZipPatch result Byte By Byte Equal ok!\n");
                } break;
                case CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_FALSE:{
                    printf("  check ZipPatch result Same Like ok! (but not Byte By Byte Equal)\n");
                } break;
                case CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_ERROR:{
                    printf("  check ZipPatch result Byte By Byte Equal ERROR!\n");
                    printf("  (did newZip=AndroidSDK#apksigner(ApkNormalized(AndroidSDK#apksigner(newZip))) before running ZipDiff?)\n");
                } break;
                case CHECK_SAME_LIKE_ERROR:{
                    printf("  check ZipPatch result zip data ERROR!\n");
                } break;
                case CHECK_ZIPPATCH_ERROR:{
                    printf("  run ZipPatch ERROR!\n");
                } break;
                default: { //CHECK_OTHER_ERROR
                    printf("  run check ZipPatch result ERROR!\n");
                }
            }
            printf("  patch time: %.3f s (thread==%d)\n",(clock_s()-time2),threadNum);
        }
    }
    if (isPatchCheck && isDiff)
        printf("\nall     time: %.3f s\n",(clock_s()-time0));
    if (!isNewZipApkV2SignNoError){ //safe check
        exitCode=1;
        printf("\nnewZip used ApkV2Sign, but not complete, as an ERROR!\n\n");
    }
    return exitCode;
}
