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
//   输入包文件,重新按固定压缩算法生成对齐的新包文件(注释、jar的签名和apk文件的v1签名会被保留,扩展字段在未压缩.so文件页面对齐时可能会被修改，apk的v2签名数据会被删除)
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
    printf("usage: ApkNormalized srcApk normalizedApk [-cl-compressLevel] [options]\n"
           "options:\n"
           "  input srcApk file can *.zip *.jar *.apk file type;\n"
           "    ApkNormalized normalized zip file:\n"
           "      recompress all compressed files's data by zlib,\n"
           "      align file data offset in zip file (compatible with AndroidSDK#zipalign),\n"
           "      remove all data descriptor, normalized Extra field & reserve Comment,\n"
           "      compatible with jar sign(apk v1 sign), etc...\n"
           "    if apk file only used apk v1 sign, don't re-sign normalizedApk file!\n"
           "    if apk file used apk v2 sign or later, must re-sign normalizedApk file after ApkNormalized;\n"
           "      release signedApk:=AndroidSDK#apksigner(normalizedApk)\n"
           "      WARNING: now, not supported Android sdk apksigner v35.\n"
           "  -cl-compressLevel\n"
           "    set zlib compress level [0..9], recommended 4,5,6, DEFAULT -cl-6;\n"
           "    NOTE: zlib not recommended 7,8,9, compress ratio is slightly higher, but\n"
           "            compress speed is very slow when patching.\n"
           "  -as-alignSize\n"
           "    set align size for uncompressed file in zip for optimize app run speed,\n"
           "    1 <= alignSize <= PageSize, recommended 4,8, DEFAULT -as-8.\n"
           "    NOTE: if not -ap-0, must PageSize%%alignSize==0;\n"
           "  -ap-pageAlignSoFile\n"
           "    if found uncompressed .so file in the zip, need align it to page size?\n"
           "      -ap-0      not page-align uncompressed .so files;\n"
           "      -ap-4k     (or -ap-1) DEFAULT, page-align uncompressed .so files to 4KB;\n"
           "                 compatible with all version of ZipPatch;\n"
           "                 WARNING: if have uncompressed .so file,\n"
           "                          it not compatible with Android 15 with 16KB page size;\n"
           "      -ap-16k    page-align uncompressed .so files to 16KB;\n"
           "                 compatible with Android 15 with 16KB page size;\n"
           "                 WARNING: if have uncompressed .so file,\n"
           "                          it can't patch by old(version<v1.8.0) ZipPatch!\n"
           "      -ap-c16k   page-align uncompressed .so files to 16KB;\n"
           "                 compatible with all version of ZipPatch,\n"
           "                 & compatible with Android 15 with 16KB page size;\n"
           "                 Note: normalized apk file size maybe larger than -ap-16k;\n"
           "      -ap-64k    page-align uncompressed .so files to 64KB;\n"
           "                 compatible with Android 15 with 16KB(or 64KB) page size;\n"
           "                 WARNING: if have uncompressed .so file,\n"
           "                          it can't patch by old(version<v1.8.0) ZipPatch!\n"
           "  -nce-isNotCompressEmptyFile\n"
           "    if found compressed empty file in the zip, need change it to not compressed?\n"
           "      -nce-0        keep the original compress setting for empty file;\n"
           "                    WARNING: if have compressed empty file,\n"
           "                             it can't patch by old(version<v1.3.5) ZipPatch!\n"
           "      -nce-1        DEFAULT, not compress all empty file.\n"
           "  -q  quiet mode, don't print fileName\n"
           "  -v  output Version info. \n"
           );
}

#define _kResult_OK                             0
#define _kResult_OPTIONS_ERROR                  1
#define _kResult_NORMALIZED_ERROR               2
#define _kResult_OPEN_NORMALIZED_ERROR          3
#define _kResult_CHECK_NORMALIZED_DATA_ERROR    4

int normalized_cmd_line(int argc, const char * argv[]);

#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
        return _kResult_OPTIONS_ERROR;
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
    if (!(value)) { printf("options " errorInfo " ERROR!\n"); printUsage(); return _kResult_OPTIONS_ERROR; } }

#define _kNULL_VALUE    (-1)
#define _kNULL_SIZE     (~(size_t)0)

int normalized_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isNotCompressEmptyFile=_kNULL_VALUE;
    size_t      pageAlignSoFile=_kNULL_SIZE;
    hpatch_BOOL pageAlignSoFileCompatible=_kNULL_VALUE;
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
            case 'q':{
                _options_check((op[2]=='\0'),"-q");
                g_isPrintNormalizingFileName=false;
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
                    _options_check((0<=compressLevel)&&(compressLevel<=9),"-cl-?");
                }else{
                    _options_check(hpatch_FALSE,"-cl?");
                }
            } break;
            case 'a':{
                if ((op[2]=='p')&&(op[3]=='-')){
                    _options_check(pageAlignSoFile==_kNULL_SIZE,"-ap-?");
                    const char* pnum=op+4;
                    pageAlignSoFileCompatible=(pnum[0]=='c')||(pnum[0]=='C');
                    if (pageAlignSoFileCompatible) ++pnum;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&pageAlignSoFile),"-ap-?");
                    #define _kMaxPageAlignSize      (64*1024)
                    #define _kDefaultPageAlignSize  (4*1024)
                    if (pageAlignSoFile==1) pageAlignSoFile=_kDefaultPageAlignSize;
                    _options_check((pageAlignSoFile==0)||(pageAlignSoFile==16*1024)
                                 ||(pageAlignSoFile==_kDefaultPageAlignSize)||(pageAlignSoFile==_kMaxPageAlignSize),"-ap-?");
                }else if ((op[2]=='s')&&(op[3]=='-')){
                    _options_check(alignSize==_kNULL_SIZE,"-as-?")
                    const char* pnum=op+4;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&alignSize),"-as-?");
                    _options_check((1<=alignSize)&&(alignSize<=_kMaxPageAlignSize),"-as-?");
                }else{
                    _options_check(hpatch_FALSE,"-a?");
                }
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    if (isNotCompressEmptyFile==_kNULL_VALUE)
        isNotCompressEmptyFile=hpatch_TRUE;
    if (pageAlignSoFile==_kNULL_SIZE)
        pageAlignSoFile=_kDefaultPageAlignSize;
    if (pageAlignSoFile==_kDefaultPageAlignSize)
        pageAlignSoFileCompatible=hpatch_TRUE;
    if (compressLevel==_kNULL_SIZE)
        compressLevel=kDefaultZlibCompressLevel;
    if (alignSize==_kNULL_SIZE)
        alignSize=kDefaultZipAlignSize;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isOutputVersion){
        printf("ApkDiffPatch::ApkNormalized v" APKDIFFPATCH_VERSION_STRING "\n\n");
        if (arg_values_size==0)
            return _kResult_OK;
    }

    if (compressLevel>6)
        printf("WARNING: not recommended 7,8,9, compress ratio is slightly higher, but compress speed is very slow when patching.\n");
        
    _options_check(arg_values_size==2,"count");
    const char* srcApk=arg_values[0];
    const char* dstApk=arg_values[1];
    hpatch_printPath_utf8((std::string("src: \"")+srcApk+"\"\n").c_str());
    hpatch_printPath_utf8((std::string("out: \"")+dstApk+"\"\n").c_str());
    double time0=clock_s();
    int apkFilesRemoved=0;
    if (!ZipNormalized(srcApk,dstApk,(int)alignSize,(int)compressLevel,
                       (bool)isNotCompressEmptyFile,pageAlignSoFile,pageAlignSoFileCompatible,&apkFilesRemoved)){
        printf("\nrun ApkNormalized ERROR!\n");
        return _kResult_NORMALIZED_ERROR;
    }
    printf("run ApkNormalized ok!\n");
    
    TCompressedFilesInfos cInfos;
    if (!getCompressedFilesInfos(dstApk,&cInfos)){
        printf("\nApkNormalized result file open ERROR!\n");
        return _kResult_OPEN_NORMALIZED_ERROR;
    }
    printf("  NOTE: recompressed files count %d, data size %" PRIu64 " compress to %" PRIu64 "\n",
            cInfos.compressedCount,cInfos.sumCompressedUncompressedSize,cInfos.sumCompressedSize);
    
    //check
    if (!getZipIsSame(srcApk,dstApk,apkFilesRemoved)){
        printf("\nApkNormalized result file check ERROR!\n");
        return _kResult_CHECK_NORMALIZED_DATA_ERROR;
    }
    printf("  check ApkNormalized result ok!\n");

    double time1=clock_s();
    printf("\nApkNormalized time: %.3f s\n",(time1-time0));
    return _kResult_OK;
}

