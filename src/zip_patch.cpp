//  zip_patch.cpp
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
#include <stdio.h>
#include <stdlib.h>
#include "../HDiffPatch/_clock_for_demo.h"
#include "../HDiffPatch/_atosize.h"
#include "patch/Patcher.h"
#include "../HDiffPatch/libParallel/parallel_import.h"
#include "patch/patch_types.h"
#include "../HDiffPatch/libHDiffPatch/HDiff/private_diff/mem_buf.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif

#ifndef PRSizeT
#   ifdef _MSC_VER
#       define PRSizeT "Iu"
#   else
#       define PRSizeT "zu"
#   endif
#endif

static void printUsage(){
    printf("usage: ZipPatch oldZipFile zipDiffFile outNewZipFile"
           " [maxUncompressMemory tempUncompressFileName] [-v]"
#if (_IS_USED_MULTITHREAD)
           " [-p-parallelThreadNumber]"
#endif
           "\noptions:\n"
           "  decompress data saved in Memory, DEFAULT or when it's size <= maxUncompressMemory;\n"
           "    if it's size > maxUncompressMemory then save it into file tempUncompressFileName;\n"
           "    maxUncompressMemory can like 8388608 8m 40m 120m 1g etc... \n"
           "  -v  output Version info. \n"
#if (_IS_USED_MULTITHREAD)
           "  -p-parallelThreadNumber\n"
           "    parallelThreadNumber DEFAULT 1 (recommended 3 etc...), if parallelThreadNumber>1\n"
           "    and used ApkV2Sign then start multi-thread Parallel compress zip; requires more memory!\n"
#endif
           );
}

#define PATCH_OPTIONS_ERROR 1
#define _options_check(value,errorInfo){ \
    if (!(value)) { printf("options " errorInfo " ERROR!\n"); printUsage(); return PATCH_OPTIONS_ERROR; } }

int zippatch_cmd_line(int argc, const char * argv[]);

#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
        return PATCH_OPTIONS_ERROR;
    SetDefaultStringLocale();
    return zippatch_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc,char* argv[]){
    return  zippatch_cmd_line(argc,(const char**)argv);
}
#   endif
#endif

#define _THREAD_NUMBER_NULL     0
#define _THREAD_NUMBER_MIN      1
#define _THREAD_NUMBER_MAX      (1<<8)

#define _kNULL_VALUE (-1)

int zippatch_cmd_line(int argc, const char * argv[]) {
    const char* oldZipPath=0;
    const char* zipDiffPath=0;
    const char* outNewZipPath=0;
    size_t      maxUncompressMemory=0;
    const char* tempUncompressFileName=0;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    size_t      threadNum = _THREAD_NUMBER_NULL;
    #define kMax_arg_values_size 5
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
            case 'v':{
                _options_check((isOutputVersion==_kNULL_VALUE)&&(op[2]=='\0'),"-v");
                isOutputVersion=hpatch_TRUE;
            } break;
#if (_IS_USED_MULTITHREAD)
            case 'p':{
                _options_check((threadNum==_THREAD_NUMBER_NULL)&&((op[2]=='-')),"-p-?");
                const char* pnum=op+3;
                _options_check(a_to_size(pnum,strlen(pnum),&threadNum),"-p-?");
                _options_check(threadNum>=_THREAD_NUMBER_MIN,"-p-?");
            } break;
#endif
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isOutputVersion){
        printf("ApkDiffPatch::ZipPatch v" APKDIFFPATCH_VERSION_STRING "\n\n");
        if (arg_values_size==0)
            return 0; //ok
    }
    if (threadNum==_THREAD_NUMBER_NULL)
        threadNum=_THREAD_NUMBER_MIN;
    else if (threadNum>_THREAD_NUMBER_MAX)
        threadNum=_THREAD_NUMBER_MAX;
    
    if(arg_values_size==3){
        oldZipPath   =arg_values[0];
        zipDiffPath  =arg_values[1];
        outNewZipPath=arg_values[2];
    }else if(arg_values_size==5){
        oldZipPath   =arg_values[0];
        zipDiffPath  =arg_values[1];
        outNewZipPath=arg_values[2];
        _options_check(kmg_to_size(arg_values[3],strlen(arg_values[3]),
                                   &maxUncompressMemory),"maxUncompressMemory");
        tempUncompressFileName=arg_values[4];
        _options_check((tempUncompressFileName!=0)
                       &&(strlen(tempUncompressFileName)>0),"tempUncompressFileName");
    }else{
        _options_check(false,"count");
    }
    printf("oldZip   :\"%s\"\nzipDiff  :\"%s\"\noutNewZip:\"%s\"\n",oldZipPath,zipDiffPath,outNewZipPath);
    if (tempUncompressFileName!=0)
        printf("maxUncompressMemory:%" PRSizeT "\ntempUncompressFileName:\"%s\"\n",
               maxUncompressMemory,tempUncompressFileName);
    
    double time0=clock_s();
    int exitCode=ZipPatch(oldZipPath,zipDiffPath,outNewZipPath,
                          maxUncompressMemory,tempUncompressFileName,(int)threadNum);
    double time1=clock_s();
    if (exitCode==PATCH_SUCCESS)
        printf("  zip file patch ok!\n");
    else
        printf("  zip file patch error!\n");
    printf("\nZipPatch time: %.3f s\n",(time1-time0));
    return exitCode;
}
