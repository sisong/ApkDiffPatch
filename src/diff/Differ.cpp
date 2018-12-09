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
#include <algorithm> //sort
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

#define _CompressPlugin_lzma //default use lzma compress diffData
#ifdef _CompressPlugin_lzma
#include "../../lzma/C/LzmaDec.h" // http://www.7-zip.org/sdk.html
#include "../../lzma/C/LzmaEnc.h"
#endif
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

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }


bool ZipDiff(const char* oldZipPath,const char* newZipPath,const char* outDiffFileName){
    TFileStreamInput    oldZipStream;
    TFileStreamInput    newZipStream;
    TFileStreamOutput   outDiffStream;
    bool            result=true;
    bool            _isInClear=false;
    
    TFileStreamInput_init(&oldZipStream);
    TFileStreamInput_init(&newZipStream);
    TFileStreamOutput_init(&outDiffStream);
    check(TFileStreamInput_open(&oldZipStream,oldZipPath));
    check(TFileStreamInput_open(&newZipStream,newZipPath));
    check(TFileStreamOutput_open(&outDiffStream,outDiffFileName,(hpatch_StreamPos_t)(-1)));
    TFileStreamOutput_setRandomOut(&outDiffStream,hpatch_TRUE);
    result=ZipDiffWithStream(&oldZipStream.base,&newZipStream.base,&outDiffStream.base);
clear:
    _isInClear=true;
    check(TFileStreamOutput_close(&outDiffStream));
    check(TFileStreamInput_close(&newZipStream));
    check(TFileStreamInput_close(&oldZipStream));
    return result;
}

#define  checkC(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=CHECK_OTHER_ERROR; if (!_isInClear){ goto clear; } } }

TCheckZipDiffResult checkZipDiff(const char* oldZipPath,const char* newZipPath,const char* diffFileName){
    TFileStreamInput    oldZipStream;
    TFileStreamInput    newZipStream;
    TFileStreamInput    diffStream;
    TCheckZipDiffResult result=CHECK_OTHER_ERROR;
    bool            _isInClear=false;
    
    TFileStreamInput_init(&oldZipStream);
    TFileStreamInput_init(&newZipStream);
    TFileStreamInput_init(&diffStream);
    checkC(TFileStreamInput_open(&oldZipStream,oldZipPath));
    checkC(TFileStreamInput_open(&newZipStream,newZipPath));
    checkC(TFileStreamInput_open(&diffStream,diffFileName));
    result=checkZipDiffWithStream(&oldZipStream.base,&newZipStream.base,&diffStream.base);
clear:
    _isInClear=true;
    checkC(TFileStreamInput_close(&diffStream));
    checkC(TFileStreamInput_close(&newZipStream));
    checkC(TFileStreamInput_close(&oldZipStream));
    return result;
}

bool ZipDiffWithStream(const hpatch_TStreamInput* oldZipStream,const hpatch_TStreamInput* newZipStream,
                       const hpatch_TStreamOutput* outDiffStream){
    UnZipper            oldZip;
    UnZipper            newZip;
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
    
    const int           myBestMatchScore=3;
#ifdef _CompressPlugin_lzma
    hdiff_TCompress*    compressPlugin=&lzmaCompressPlugin;
    hpatch_TDecompress* decompressPlugin=&lzmaDecompressPlugin;
    lzma_compress_level =9;
    lzma_dictSize       =4*1024*1024;
#else
    hdiff_TCompress*    compressPlugin=&zlibCompressPlugin;
    hpatch_TDecompress* decompressPlugin=&zlibDecompressPlugin;
    zlib_compress_level =9; //0..9
#endif
    
    UnZipper_init(&oldZip);
    UnZipper_init(&newZip);
    check(UnZipper_openStream(&oldZip,oldZipStream));
    check(UnZipper_openStream(&newZip,newZipStream));
    
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
        std::sort(newRefDecompressList.begin(),newRefDecompressList.end());
    }
    check(getOldRefList(&newZip,samePairList,newRefDecompressList,&oldZip,oldRefList));
    std::cout<<"ZipDiff same file count: "<<samePairList.size()/2<<" (all "
    <<UnZipper_fileCount(&newZip)<<")\n";
    std::cout<<"    diff new file count: "<<newRefList.size()+newRefOtherCompressedList.size()<<"\n";
    std::cout<<"     ref old file count: "<<oldRefList.size()<<" (all "
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
    std::cout<<"ZipDiff size: "<<out_diffData.size()<<"\n";
    
    check((long)out_diffData.size()==outDiffStream->write(outDiffStream->streamHandle,0,out_diffData.data(),
                                                          out_diffData.data()+out_diffData.size()));
clear:
    _isInClear=true;
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
        std::cout<<"\n  patch check HDiffZ result error!!!\n";
        return false;
    }else{
        double time2=clock_s();
        std::cout<<"  patch check HDiffZ result ok!\n";
        std::cout<<"  patch time: "<<(time2-time1)<<" s\n\n";
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


static bool getIsEqual(const hpatch_TStreamInput* x,const hpatch_TStreamInput* y){
    size_t dataSize=(size_t)x->streamSize;
    assert(dataSize==x->streamSize);
    assert((((size_t)(dataSize<<1))>>1) == dataSize);
    if (dataSize!=y->streamSize)
        return false;
    if (dataSize>0){
        std::vector<TByte> _buf;
        _buf.resize(dataSize*2);
        TByte* buf=_buf.data();
        if ((long)dataSize!=x->read(x->streamHandle,0,buf,buf+dataSize)) return false;
        if ((long)dataSize!=y->read(y->streamHandle,0,buf+dataSize,buf+dataSize*2)) return false;
        if (0!=memcmp(buf,buf+dataSize,dataSize)) return false;
    }
    return true;
}


    struct TVectorStreamOutput:public hpatch_TStreamOutput{
        explicit TVectorStreamOutput(std::vector<TByte>& _dst):dst(_dst){
            this->streamHandle=this;
            this->streamSize=-1;
            this->write=_write;
        }
        static long _write(hpatch_TStreamOutputHandle streamHandle,
                           const hpatch_StreamPos_t writeToPos,
                           const unsigned char* data,const unsigned char* data_end){
            TVectorStreamOutput* self=(TVectorStreamOutput*)streamHandle;
            std::vector<TByte>& dst=self->dst;
            size_t writeLen=(size_t)(data_end-data);
            assert(writeToPos<=dst.size());
            if  (dst.size()==writeToPos){
                dst.insert(dst.end(),data,data_end);
            }else{
                assert((size_t)(writeToPos+writeLen)==writeToPos+writeLen);
                if (dst.size()<writeToPos+writeLen)
                    dst.resize((size_t)(writeToPos+writeLen));
                memcpy(&dst[(size_t)writeToPos],data,writeLen);
            }
            return (long)writeLen;
        }
        std::vector<TByte>& dst;
    };

TCheckZipDiffResult checkZipDiffWithStream(const hpatch_TStreamInput* oldZipStream,
                                           const hpatch_TStreamInput* newZipStream,
                                           const hpatch_TStreamInput* diffStream){
    assert((size_t)newZipStream->streamSize==newZipStream->streamSize);
    std::vector<TByte>      temp_newZip;
    TVectorStreamOutput     temp_outNewZipStream(temp_newZip);
    if (PATCH_SUCCESS!=ZipPatchWithStream(oldZipStream,diffStream,&temp_outNewZipStream,0,0))
        return CHECK_ZIPPATCH_ERROR;
    //else
        
    
    hpatch_TStreamInput     temp_inNewZipStream;
    mem_as_hStreamInput(&temp_inNewZipStream,temp_newZip.data(),temp_newZip.data()+temp_newZip.size());
    bool isByteByByteEqual=getIsEqual(newZipStream,&temp_inNewZipStream);
    if (isByteByByteEqual)
        return CHECK_BYTE_BY_BYTE_EQUAL_TRUE;
    
    bool isOldHaveApkV2Sign=false;
    bool isSame=getZipIsSameWithStream(newZipStream,&temp_inNewZipStream,&isOldHaveApkV2Sign);
    if (!isSame)
        return CHECK_SAME_LIKE_ERROR;
    if (isOldHaveApkV2Sign)
        return CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_ERROR;
    else
        return CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_FALSE;
}
