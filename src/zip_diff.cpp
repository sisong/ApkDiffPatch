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
#include <iostream>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diff/Differ.h"
#include "patch/patch_types.h"
#include "../HDiffPatch/_clock_for_demo.h"

static void printUsage(){
    printf("diff usage: ZipDiff oldZipFile newZipFile outDiffFileName\n"
           "test usage: ZipDiff -t oldZipFile newZipFile testDiffFileName\n"
           "options:\n"
           "  input oldZipFile & newZipFile file can *.zip *.jar *.apk file type;\n"
           "  -v  output Version info. \n"
           "  -t  Test only, run patch check, ZipPatch(oldZipFile,testDiffFile)==newZipFile ? \n");
}

#define ZIPDIFF_OPTIONS_ERROR 1
#define _options_check(value,errorInfo){ \
    if (!(value)) { printf("options " errorInfo " ERROR!\n"); printUsage(); return ZIPDIFF_OPTIONS_ERROR; } }

#define _kNULL_VALUE (-1)

int main(int argc, const char * argv[]) {
    const char* oldZipPath     =0;
    const char* newZipPath     =0;
    const char* outDiffFileName=0;
    hpatch_BOOL isPatchCheck=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
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
            case 'v':{
                _options_check((isOutputVersion==_kNULL_VALUE)&&(op[2]=='\0'),"-v");
                isOutputVersion=hpatch_TRUE;
            } break;
            case 't':{
                _options_check((isPatchCheck==_kNULL_VALUE)&&(op[2]=='\0'),"-t");
                isPatchCheck=hpatch_TRUE; //test diffFile
            } break;
#if (IS_USED_MULTITHREAD)
            case 'p':{
                _options_check((threadNum==THREAD_NUMBER_NULL)&&((op[2]=='-')),"-p-?");
                const char* pnum=op+3;
                _options_check(a_to_size(pnum,strlen(pnum),&threadNum),"-p-?");
                _options_check(threadNum>=THREAD_NUMBER_DEFAULT,"-p-?");
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
        printf("ApkDiffPatch::ZipDiff v" APKDIFFPATCH_VERSION_STRING "\n\n");
        if (arg_values_size==0)
            return 0; //ok
    }
    
    hpatch_BOOL isDiff=_kNULL_VALUE;
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
    if (isDiff){
        if (!ZipDiff(oldZipPath,newZipPath,outDiffFileName)){
            printf("ZipDiff error!\n");
            return 1;
        }//else
        printf("ZipDiff time: %.3f s\n",(clock_s()-time0));
    }
    
    int exitCode=0;
    if (isPatchCheck){
        double time2=clock_s();
        printf("\nrun ZipPatch:\n");
        TCheckZipDiffResult rt=checkZipDiff(oldZipPath,newZipPath,outDiffFileName);
        exitCode=((rt==CHECK_BYTE_BY_BYTE_EQUAL_TRUE)||(rt==CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_FALSE))?0:1;
        switch (rt) {
            case CHECK_BYTE_BY_BYTE_EQUAL_TRUE:{
                printf("  check ZipPatch result Byte By Byte Equal ok!\n");
            } break;
            case CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_FALSE:{
                printf("  check ZipPatch result Same Like ok! (but not Byte By Byte Equal)\n");
            } break;
            case CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_ERROR:{
                printf("  check ZipPatch result Byte By Byte Equal ERROR!)\n");
            } break;
            case CHECK_SAME_LIKE_ERROR:{
                printf("  check ZipPatch result Same Like ERROR!)\n");
            } break;
            case CHECK_ZIPPATCH_ERROR:{
                printf("  run ZipPatch ERROR!)\n");
            } break;
            default: { //CHECK_OTHER_ERROR
                printf("  run check ZipPatch result ERROR!)\n");
            }
        }
        printf("  patch time: %.3f s\n",(clock_s()-time2));
    }
    if (isPatchCheck && isDiff)
        printf("\nall     time: %.3f s\n",(clock_s()-time0));
    return exitCode;
}
