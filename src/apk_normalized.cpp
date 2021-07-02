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
//  $apksigner sign --v1-signing-enabled true --v2-signing-enabled true --ks *.keystore --ks-pass pass:* --in normalized.apk --out release.apk

#include "normalized/normalized.h"
#include "../HDiffPatch/_clock_for_demo.h"
#include "../HDiffPatch/_atosize.h"
#include "../HDiffPatch/libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "diff/DiffData.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif

static void printUsage(){
    printf("usage: ApkNormalized srcApk out_newApk [-nce-isNotCompressEmptyFile] [-cl-compressLevel] [-as-alignSize] [-v] \n"
           "options:\n"
           "  input srcApk file can *.zip *.jar *.apk file type;\n"
           "    ApkNormalized normalized zip file:\n"
           "      recompress all compressed files's data by zlib,\n"
           "      align file data offset in zip file (compatible with AndroidSDK#zipalign),\n"
           "      remove all data descriptor, reserve & normalized Extra field and Comment,\n"
           "      compatible with jar sign(apk v1 sign), etc...\n"
           "    if apk file used apk v2 sign, must re sign apk file after ApkNormalized;\n"
           "      release newApk:=AndroidSDK#apksigner(out_newApk)\n"
           "  -nce-isNotCompressEmptyFile\n"
           "    if found compressed empty file in the zip, need change it to not compressed?\n"
           "      -nce-0        keep the original compress setting for empty file;\n"
           "                      WARNING: if have compressed empty file,\n"
           "                      it can't patch by old ZipPatch(version<v1.3.5)!\n"
           "      -nce-1        DEFAULT, not compress all empty file.\n"
           "  -cl-compressLevel\n"
           "    set zlib compress level, 1<=compressLevel<=9, DEFAULT -cl-6;\n"
           "    if set 9, compress rate is high, but compress speed is very slow when patching.\n"
           "  -as-alignSize\n"
           "    set align size for uncompressed file in zip for optimize app run speed,\n"
           "    alignSize>=1, recommended 4,8, DEFAULT -as-8.\n"
           "  -v  output Version info. \n"
           );
}

#define _kOPTIONS_ERROR 1
int normalized_cmd_line(int argc, const char * argv[]);

#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
        return _kOPTIONS_ERROR;
    SetDefaultStringLocale();
    return normalized_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc,char* argv[]){
    return  normalized_cmd_line(argc,(const char**)argv);
}
#   endif
#endif

#define _options_check(value,errorInfo){ \
    if (!(value)) { printf("options " errorInfo " ERROR!\n"); printUsage(); return _kOPTIONS_ERROR; } }

#define _kNULL_VALUE    (-1)
#define _kNULL_SIZE     (~(size_t)0)

int normalized_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isNotCompressEmptyFile=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    size_t      compressLevel = _kNULL_SIZE;
    size_t      alignSize     = _kNULL_SIZE;
#define kMax_arg_values_size 2
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
            case 'n':{
                if ((op[2]=='c')&&(op[3]=='e')&&(op[4]=='-')&&((op[5]=='0')||(op[5]=='1'))){
                    _options_check(isNotCompressEmptyFile==_kNULL_VALUE,"-nce-?");
                    isNotCompressEmptyFile=(hpatch_BOOL)(op[5]=='1');
                }else{
                    _options_check(hpatch_FALSE,"-nce?");
                }
            } break;
            case 'c':{
                if ((op[2]=='l')&&(op[3]=='-')){
                    _options_check(compressLevel==_kNULL_SIZE,"-cl-?")
                    const char* pnum=op+4;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&compressLevel),"-cl-?");
                    _options_check((1<=compressLevel)&&(compressLevel<=9),"-cl-?");
                }else{
                    _options_check(hpatch_FALSE,"-cl?");
                }
            } break;
            case 'a':{
                if ((op[2]=='s')&&(op[3]=='-')){
                    _options_check(alignSize==_kNULL_SIZE,"-as-?")
                    const char* pnum=op+4;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&alignSize),"-as-?");
                    const size_t _kMaxAlignSize=(1<<20);
                    _options_check((1<=alignSize)&&(alignSize<=_kMaxAlignSize),"-as-?");
                }else{
                    _options_check(hpatch_FALSE,"-as?");
                }
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    if (isNotCompressEmptyFile==_kNULL_VALUE)
        isNotCompressEmptyFile=hpatch_TRUE;
    if (compressLevel==_kNULL_SIZE)
        compressLevel=kDefaultZlibCompressLevel;
    if (alignSize==_kNULL_SIZE)
        alignSize=kDefaultZipAlignSize;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isOutputVersion){
        printf("ApkDiffPatch::ApkNormalized v" APKDIFFPATCH_VERSION_STRING "\n\n");
        if (arg_values_size==0)
            return 0; //ok
    }
    
    _options_check(arg_values_size==2,"count");
    const char* srcApk=arg_values[0];
    const char* dstApk=arg_values[1];
    printf("src: \"%s\"\nout: \"%s\"\n",srcApk,dstApk);
    double time0=clock_s();
    if (!ZipNormalized(srcApk,dstApk,(int)alignSize,(int)compressLevel,isNotCompressEmptyFile?true:false)){
        printf("\nrun ApkNormalized ERROR!\n");
        return 1;
    }
    printf("run ApkNormalized ok!\n");
    
    //check
    if (!getZipIsSame(srcApk,dstApk)){
        printf("ApkNormalized result file check ERROR!\n");
        return 1;
    }
    printf("  check ApkNormalized result ok!\n");
    
    double time1=clock_s();
    printf("\nApkNormalized time: %.3f s\n",(time1-time0));
    return 0;
}

