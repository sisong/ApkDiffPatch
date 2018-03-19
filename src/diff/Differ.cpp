//  Differ.cpp
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
#include "Differ.h"
#include <iostream>
#include <stdio.h>
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff.h"  //https://github.com/sisong/HDiffPatch
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch.h"
#include "../../HDiffPatch/file_for_patch.h"
#include "../../HDiffPatch/_clock_for_demo.h"
#include "../patch/OldStream.h"
#include "../patch/Patcher.h"
#include "OldRef.h"
#include "DiffData.h"
#include "DiffData.h"
#include "../patch/patch_types.h"
#define _CompressPlugin_lzma //use lzma for compress out diffData
#include "../../lzma/C/LzmaDec.h" // http://www.7-zip.org/sdk.html
#include "../../lzma/C/LzmaEnc.h" // http://www.7-zip.org/sdk.html
#include "../../HDiffPatch/compress_plugin_demo.h"
#include "../../HDiffPatch/decompress_plugin_demo.h"

//* for close some compiler warning :(
#ifdef _CompressPlugin_lzma
hdiff_TCompress*       __not_used_for_compiler__null0 =&lzmaCompressPlugin;
hpatch_TDecompress*    __not_used_for_compiler__null1 =&lzmaDecompressPlugin;
hdiff_TStreamCompress* __not_used_for_compiler__null2 =&lzmaStreamCompressPlugin;
#endif
hdiff_TCompress*       __not_used_for_compiler__null3 =&zlibCompressPlugin;
hpatch_TDecompress*    __not_used_for_compiler__null4 =&zlibDecompressPlugin;
hdiff_TStreamCompress* __not_used_for_compiler__null5 =&zlibStreamCompressPlugin;
//*/

bool HDiffZ(const std::vector<TByte>& oldData,const std::vector<TByte>& newData,std::vector<TByte>& out_diffData,
            hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,int myBestMatchScore);
static bool checkZipInfo(UnZipper* oldZip,UnZipper* newZip);
static bool testZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath);
static bool getFileIsEqual(const char* xFileName,const char* yFileName);

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }


bool ZipDiff(const char* oldZipPath,const char* newZipPath,const char* outDiffFileName,
             const char* temp_ZipPatchFileName){
    const int           myBestMatchScore=5;
    UnZipper            oldZip;
    UnZipper            newZip;
    TFileStreamOutput   out_diffFile;
    std::vector<TByte>  newData;
    std::vector<TByte>  oldData;
    std::vector<TByte>  hdiffzData;
    std::vector<TByte>  out_diffData;
    std::vector<uint32_t> samePairList;
    std::vector<uint32_t> newRefDecompressList;
    std::vector<uint32_t> newRefList;
    std::vector<uint32_t> newRefOtherCompressedList;
    std::vector<uint32_t> newRefCompressedSizeList;
    std::vector<uint32_t> oldRefList;
    bool            result=true;
    bool            _isInClear=false;
    bool            byteByByteEqualCheck=false;
    size_t          newZipAlignSize=0;
    int             newZipNormalized_compressLevel=kDefaultZlibCompressLevel;
    int             newZipNormalized_compressMemLevel=kDefaultZlibCompressMemLevel;
    int             newZip_otherCompressLevel=0;
    int             newZip_otherCompressMemLevel=0;
    bool            newCompressedDataIsNormalized=false;
#ifdef _CompressPlugin_lzma
    //lzma_compress_level: hdiffpatch default
    //lzma_dictSize: hdiffpatch default
    hdiff_TCompress* compressPlugin=&lzmaCompressPlugin;
    hpatch_TDecompress* decompressPlugin=&lzmaDecompressPlugin;
#else
    zlib_compress_level=9; //0..9
    hdiff_TCompress* compressPlugin=&zlibCompressPlugin;
    hpatch_TDecompress* decompressPlugin=&zlibDecompressPlugin;
#endif
    UnZipper_init(&oldZip);
    UnZipper_init(&newZip);
    TFileStreamOutput_init(&out_diffFile);
    check(UnZipper_openRead(&oldZip,oldZipPath));
    check(UnZipper_openRead(&newZip,newZipPath));
    
    newZipAlignSize=getZipAlignSize_unsafe(&newZip);
    if (UnZipper_isHaveApkV2Sign(&newZip)){//precondition (+checkZipIsSame() to complete)
        newZip._isDataNormalized=(newZipAlignSize>0)&(newZip._dataDescriptorCount==0);
    }else{
        newZip._isDataNormalized=true;
    }
    newZipAlignSize=(newZipAlignSize>0)?newZipAlignSize:kDefaultZipAlignSize;
    if (newZip._isDataNormalized && UnZipper_isHaveApkV2Sign(&newZip)){
        newCompressedDataIsNormalized=getCompressedIsNormalized(&newZip,&newZipNormalized_compressLevel,
                                                                &newZipNormalized_compressMemLevel);
    }else{
        newCompressedDataIsNormalized=getCompressedIsNormalizedBy(&newZip,newZipNormalized_compressLevel,
                                                                  newZipNormalized_compressMemLevel);
    }
    newZip._isDataNormalized&=newCompressedDataIsNormalized;
    if (UnZipper_isHaveApkV2Sign(&newZip)){
        if (!getCompressedIsNormalized(&newZip,&newZip_otherCompressLevel,&newZip_otherCompressMemLevel,true)){
            newZip_otherCompressLevel=0;
            newZip_otherCompressMemLevel=0;
        }
    }
    byteByByteEqualCheck=UnZipper_isHaveApkV2Sign(&newZip);
    
    if (newCompressedDataIsNormalized && UnZipper_isHaveApkV2Sign(&oldZip))
        oldZip._isDataNormalized=getCompressedIsNormalizedBy(&oldZip,newZipNormalized_compressLevel,
                                                             newZipNormalized_compressMemLevel);
    check(checkZipInfo(&oldZip,&newZip));
    
    std::cout<<"ZipDiff with compress plugin: \""<<compressPlugin->compressType(compressPlugin)<<"\"\n";
    check(getSamePairList(&newZip,&oldZip,newCompressedDataIsNormalized,
                          newZipNormalized_compressLevel,newZipNormalized_compressMemLevel,
                          samePairList,newRefList,newRefOtherCompressedList,newRefCompressedSizeList));
    newRefDecompressList=newRefList;
    if ((newZip_otherCompressLevel|newZip_otherCompressMemLevel)!=0){
        newRefDecompressList.insert(newRefDecompressList.end(),
                                    newRefOtherCompressedList.begin(),newRefOtherCompressedList.end());
    }
    check(getOldRefList(&newZip,samePairList,newRefDecompressList,&oldZip,oldRefList));
    std::cout<<"ZipDiff same file count: "<<samePairList.size()/2<<"\n";
    std::cout<<"    diff new file count: "<<newRefList.size()+newRefOtherCompressedList.size()<<"\n";
    std::cout<<"     ref old file count: "<<oldRefList.size()<<" ("
        <<UnZipper_fileCount(&oldZip)<<")\n";
    std::cout<<"     ref old decompress: "
        <<OldStream_getDecompressFileCount(&oldZip,oldRefList.data(),oldRefList.size())<<" file ("
        <<OldStream_getDecompressSumSize(&oldZip,oldRefList.data(),oldRefList.size()) <<" byte!)\n";
    //for (int i=0; i<(int)newRefList.size(); ++i) std::cout<<zipFile_name(&newZip,newRefList[i])<<"\n";
    //for (int i=0; i<(int)newRefOtherCompressedList.size(); ++i) std::cout<<zipFile_name(&newZip,newRefOtherCompressedList[i])<<"\n";
    //for (int i=0; i<(int)oldRefList.size(); ++i) std::cout<<zipFile_name(&oldZip,oldRefList[i])<<"\n";

    if ((newZip_otherCompressLevel|newZip_otherCompressMemLevel)!=0){
        check(readZipStreamData(&newZip,newRefDecompressList,std::vector<uint32_t>(),newData));
    }else{
        check(readZipStreamData(&newZip,newRefList,newRefOtherCompressedList,newData));
    }
    check(readZipStreamData(&oldZip,oldRefList,std::vector<uint32_t>(),oldData));
    check(HDiffZ(oldData,newData,hdiffzData,compressPlugin,decompressPlugin,myBestMatchScore));
    { std::vector<TByte> _empty; oldData.swap(_empty); }
    { std::vector<TByte> _empty; newData.swap(_empty); }
    
    check(serializeZipDiffData(out_diffData,&newZip,&oldZip,newZipAlignSize,
                               newZipNormalized_compressLevel,newZipNormalized_compressMemLevel,
                               newZip_otherCompressLevel,newZip_otherCompressMemLevel,
                               samePairList,newRefOtherCompressedList,newRefCompressedSizeList,
                               oldRefList,hdiffzData,compressPlugin));
    std::cout<<"\nZipDiff size: "<<out_diffData.size()<<"\n";

    check(TFileStreamOutput_open(&out_diffFile,outDiffFileName,out_diffData.size()));
    check((long)out_diffData.size()==out_diffFile.base.write(out_diffFile.base.streamHandle,
                                        0,out_diffData.data(),out_diffData.data()+out_diffData.size()));
    check(TFileStreamOutput_close(&out_diffFile));
    std::cout<<"  out ZipDiff file ok!\n";
    check(UnZipper_close(&newZip));
    check(UnZipper_close(&oldZip));
    
    check(testZipPatch(oldZipPath,outDiffFileName,temp_ZipPatchFileName));

    if (byteByByteEqualCheck){
       check(getFileIsEqual(newZipPath,temp_ZipPatchFileName));
        std::cout<<"  check ZipPatch result Equal ok!\n";
    }else{
       check(getZipIsSame(newZipPath,temp_ZipPatchFileName));
       std::cout<<"  check ZipPatch result Same ok!\n";
    }
    
clear:
    _isInClear=true;
    check(TFileStreamOutput_close(&out_diffFile));
    check(UnZipper_close(&newZip));
    check(UnZipper_close(&oldZip));
    return result;
}

bool HDiffZ(const std::vector<TByte>& oldData,const std::vector<TByte>& newData,std::vector<TByte>& out_diffData,
            hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,int myBestMatchScore){
    double time0=clock_s();
    const size_t oldDataSize=oldData.size();
    const size_t newDataSize=newData.size();
    std::cout<<"\nrun HDiffZ:\n";
    std::cout<<"  oldDataSize : "<<oldDataSize<<"\n  newDataSize : "<<newDataSize<<"\n";
    
    std::vector<TByte>& diffData=out_diffData;
    const TByte* newData0=newData.data();
    const TByte* oldData0=oldData.data();
    create_compressed_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                           diffData,compressPlugin,myBestMatchScore);
    double time1=clock_s();
    std::cout<<"  diffDataSize: "<<diffData.size()<<"\n";
    std::cout<<"  diff  time: "<<(time1-time0)<<" s\n";
    if (!check_compressed_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                               diffData.data(),diffData.data()+diffData.size(),decompressPlugin)){
        std::cout<<"\n  HPatch check HDiffZ result error!!!\n";
        return false;
    }else{
        double time2=clock_s();
        std::cout<<"  HPatch check HDiffZ result ok!\n";
        std::cout<<"  hpatch time: "<<(time2-time1)<<" s\n";
        return true;
    }
}

static bool checkZipInfo(UnZipper* oldZip,UnZipper* newZip){
    if (oldZip->_isDataNormalized)
        printf("  NOTE: oldZip Normalized\n");
    if (UnZipper_isHaveApkV1_or_jarSign(oldZip))
        printf("  NOTE: oldZip found JarSign(ApkV1Sign)\n");
    if (UnZipper_isHaveApkV2Sign(oldZip))
        printf("  NOTE: oldZip found ApkV2Sign\n");
    if (newZip->_isDataNormalized)
        printf("  NOTE: newZip Normalized\n");
    if (UnZipper_isHaveApkV1_or_jarSign(newZip))
        printf("  NOTE: newZip found JarSign(ApkV1Sign)\n");
    bool newIsV2Sign=UnZipper_isHaveApkV2Sign(newZip);
    if (newIsV2Sign)
        printf("  NOTE: newZip found ApkV2Sign\n");
    
    if (newIsV2Sign&(!newZip->_isDataNormalized)){
        printf("  ERROR: newZip not Normalized, need run newZip=AndroidSDK#apksigner(ApkNormalized(newZip)) before run ZipDiff!\n");
        //return false;
    }
    return true;
}

static bool testZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath){
    double time0=clock_s();
    TPatchResult ret=ZipPatch(oldZipPath,zipDiffPath,outNewZipPath,0,0);
    double time1=clock_s();
    if (ret==PATCH_SUCCESS){
        std::cout<<"\nrun ZipPatch ok!\n";
        std::cout<<"  patch time: "<<(time1-time0)<<" s\n";
        return true;
    }else{
        return false;
    }
}


static bool getFileIsEqual(const char* xFileName,const char* yFileName){
    TFileStreamInput x;
    TFileStreamInput y;
    bool            result=true;
    bool            _isInClear=false;
    std::vector<TByte> buf;
    size_t          fileSize;
    TFileStreamInput_init(&x);
    TFileStreamInput_init(&y);
    check(TFileStreamInput_open(&x,xFileName));
    check(TFileStreamInput_open(&y,yFileName));
    fileSize=(size_t)x.base.streamSize;
    assert(fileSize==x.base.streamSize);
    check(fileSize==y.base.streamSize);
    if (fileSize>0){
        buf.resize(fileSize*2);
        check((long)fileSize==x.base.read(x.base.streamHandle,0,buf.data(),buf.data()+fileSize));
        check((long)fileSize==y.base.read(y.base.streamHandle,0,buf.data()+fileSize,buf.data()+fileSize*2));
        check(0==memcmp(buf.data(),buf.data()+fileSize,fileSize));
    }
clear:
    _isInClear=true;
    TFileStreamInput_close(&x);
    TFileStreamInput_close(&y);
    return result;
}

