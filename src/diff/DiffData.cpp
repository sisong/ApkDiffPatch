//  DiffData.cpp
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
#include "DiffData.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
#include "../patch/OldStream.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/private_diff/pack_uint.h"
using namespace hdiff_private;

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }

#define  test_clear(value) { \
    if (!(value)){ \
        result=false; if (!_isInClear){ goto clear; } } }

bool zipFileData_isSame(UnZipper* selfZip,int selfIndex,UnZipper* srcZip,int srcIndex){
    uint32_t selfFileSize=UnZipper_file_uncompressedSize(selfZip,selfIndex);
    if (selfFileSize!=UnZipper_file_uncompressedSize(srcZip,srcIndex)) return false;
    std::vector<TByte> buf(selfFileSize*2);
    hpatch_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,buf.data(),buf.data()+selfFileSize);
    check(UnZipper_fileData_decompressTo(selfZip,selfIndex,&stream));
    mem_as_hStreamOutput(&stream,buf.data()+selfFileSize,buf.data()+selfFileSize*2);
    check(UnZipper_fileData_decompressTo(srcZip,srcIndex,&stream));
    return 0==memcmp(buf.data(),buf.data()+selfFileSize,selfFileSize);
}

bool getZipIsSame(const char* oldZipPath,const char* newZipPath){
    UnZipper            oldZip;
    UnZipper            newZip;
    bool            result=true;
    bool            _isInClear=false;
    int             fileCount=0;
    
    UnZipper_init(&oldZip);
    UnZipper_init(&newZip);
    test_clear(UnZipper_openRead(&oldZip,oldZipPath));
    test_clear(UnZipper_openRead(&newZip,newZipPath));
    
    fileCount=UnZipper_fileCount(&oldZip);
    test_clear(fileCount=UnZipper_fileCount(&newZip));
    for (int i=0;i<fileCount; ++i) {
        test_clear(zipFile_name(&oldZip,i)==zipFile_name(&newZip,i));
        test_clear(UnZipper_file_crc32(&oldZip,i)==UnZipper_file_crc32(&newZip,i));
        test_clear(zipFileData_isSame(&oldZip,i,&newZip,i));
    }
clear:
    _isInClear=true;
    test_clear(UnZipper_close(&newZip));
    test_clear(UnZipper_close(&oldZip));
    return result;
}

bool getNormalizedCompressedSize(UnZipper* selfZip,int selfIndex,uint32_t* out_compressedSize){
    uint32_t selfFileSize=UnZipper_file_uncompressedSize(selfZip,selfIndex);
    uint32_t maxCodeSize=Zipper_compressData_maxCodeSize(selfFileSize);
    std::vector<TByte> buf(selfFileSize+maxCodeSize);
    hpatch_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,buf.data(),buf.data()+selfFileSize);
    check(UnZipper_fileData_decompressTo(selfZip,selfIndex,&stream));
    *out_compressedSize=Zipper_compressData(buf.data(),selfFileSize,buf.data()+selfFileSize,maxCodeSize);
    return (*out_compressedSize)>0;
}

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,
                     std::vector<uint32_t>& out_samePairList,
                     std::vector<uint32_t>& out_newRefList,
                     std::vector<uint32_t>* out_newReCompressList){
    int oldFileCount=UnZipper_fileCount(oldZip);
    typedef std::multimap<uint32_t,int> TMap;
    TMap crcMap;
    for (int i=0; i<oldFileCount; ++i) {
        uint32_t crcOld=UnZipper_file_crc32(oldZip,i);
        crcMap.insert(TMap::value_type(crcOld,i));
    }
    
    int newFileCount=UnZipper_fileCount(newZip);
    for (int i=0; i<newFileCount; ++i) {
        bool findSame=false;
        if (!UnZipper_file_isApkV2Compressed(newZip,i)){
            uint32_t crcNew=UnZipper_file_crc32(newZip,i);
            std::pair<TMap::const_iterator,TMap::const_iterator> range=crcMap.equal_range(crcNew);
            for (;range.first!=range.second;++range.first) {
                int oldIndex=range.first->second;
                if (zipFileData_isSame(newZip,i,oldZip,oldIndex)){
                    if (UnZipper_file_isApkV2Compressed(oldZip,oldIndex)) continue;
                    findSame=true;
                    out_samePairList.push_back(i);
                    out_samePairList.push_back(oldIndex);
                    break;
                }else{
                    printf("WARNING: crc32 equal but data not equal! file index: %d,%d\n",i,oldIndex);
                    printf("   name:\"%s\"\n        \"%s\"\n",zipFile_name(newZip,i).c_str(),
                           zipFile_name(oldZip,oldIndex).c_str());
                }
            }
        }
        if (!findSame){
            out_newRefList.push_back(i);
            if ((out_newReCompressList!=0)&&UnZipper_file_isCompressed(newZip,i)
                  &&(!UnZipper_file_isApkV2Compressed(newZip,i))){
                uint32_t compressedSize=0;
                check(getNormalizedCompressedSize(newZip,i,&compressedSize));
                out_newReCompressList->push_back(compressedSize);
            }
        }
    }
    return true;
}

struct t_auto_OldStream {
    inline t_auto_OldStream(OldStream* p):_p(p){}
    inline ~t_auto_OldStream(){ OldStream_close(_p); }
    OldStream* _p;
};

bool readZipStreamData(UnZipper* zip,const std::vector<uint32_t>& refList,std::vector<unsigned char>& out_data){
    long outSize=0;
    OldStream stream;
    OldStream_init(&stream);
    t_auto_OldStream _t_auto_OldStream(&stream);
    
    ZipFilePos_t decompressSumSize=OldStream_getDecompressSumSize(zip,refList.data(),refList.size());
    std::vector<TByte> decompressData(decompressSumSize);
    hpatch_TStreamOutput out_decompressStream;
    hpatch_TStreamInput  in_decompressStream;
    mem_as_hStreamOutput(&out_decompressStream,decompressData.data(),decompressData.data()+decompressSumSize);
    mem_as_hStreamInput(&in_decompressStream,decompressData.data(),decompressData.data()+decompressSumSize);
    check(OldStream_getDecompressData(zip,refList.data(),refList.size(),&out_decompressStream));
    check(OldStream_open(&stream,zip,refList.data(),refList.size(),&in_decompressStream));
    
    outSize=stream.stream->streamSize;
    assert(outSize==stream.stream->streamSize);
    out_data.resize(outSize);
    check(outSize==stream.stream->read(stream.stream->streamHandle,0,out_data.data(),out_data.data()+outSize));
    return true;
}


static bool _serializeZipDiffData(std::vector<TByte>& out_data,const ZipDiffData*  data,
                                  const std::vector<TByte>& hdiffzData,hdiff_TCompress* compressPlugin){
    std::vector<TByte> headData;
    {//head data
        for (size_t i=0;i<data->reCompressCount;++i){
            packUInt(headData,(uint32_t)data->reCompressList[i]);
        }
        uint32_t backValue=-1;
        for (size_t i=0;i<data->refCount;++i){
            uint32_t curValue=data->refList[i];
            packUInt(headData,(uint32_t)(curValue-(uint32_t)(backValue+1)));
            backValue=curValue;
        }
        uint32_t backPairNew=-1;
        uint32_t backPairOld=-1;
        for (size_t i=0;i<data->samePairCount;++i){
            uint32_t curNewValue=data->samePairList[i*2+0];
            packUInt(headData,(uint32_t)(curNewValue-(uint32_t)(backPairNew+1)));
            backPairNew=curNewValue;
            
            uint32_t curOldValue=data->samePairList[i*2+1];
            if (curOldValue>=(uint32_t)(backPairOld+1))
                packUIntWithTag(headData,(uint32_t)(curOldValue-(uint32_t)(backPairOld+1)),0,1);
            else
                packUIntWithTag(headData,(uint32_t)((uint32_t)(backPairOld+1)-curOldValue),1,1);
            backPairOld=curOldValue;
        }
    }
    std::vector<TByte> headCode;
    {
        headCode.resize(compressPlugin->maxCompressedSize(compressPlugin,headData.size()));
        size_t codeSize=compressPlugin->compress(compressPlugin,
                                                 headCode.data(),headCode.data()+headCode.size(),
                                                 headData.data(),headData.data()+headData.size());
        if ((0<codeSize)&(codeSize<=headCode.size()))
            headCode.resize(codeSize);
        else
            return false;//error
    }
    
    {//type version
        static const char* kVersionType="ZipDiff1&";
        #define kVersionTypeLen 9
        pushBack(out_data,(const TByte*)kVersionType,(const TByte*)kVersionType+kVersionTypeLen);
    }
    {//compressType
        const char* compressType=compressPlugin->compressType(compressPlugin);
        size_t compressTypeLen=strlen(compressType);
        if (compressTypeLen>hpatch_kMaxCompressTypeLength) return false;
        pushBack(out_data,(const TByte*)compressType,(const TByte*)compressType+compressTypeLen+1); //'\0'
    }
    //head info
    packUInt(out_data,data->newZipFileCount);
    packUInt(out_data,data->newZipIsNormalized);
    packUInt(out_data,data->newZipVCESize);
    packUInt(out_data,data->reCompressCount);
    packUInt(out_data,data->samePairCount);
    packUInt(out_data,data->oldZipVCESize);
    packUInt(out_data,data->refCount);
    packUInt(out_data,data->refSumSize);
    packUInt(out_data,data->oldCrc);
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    packUInt(out_data,hdiffzData.size());
    
    //code data
    headData.clear();
    pushBack(out_data,headCode);
    headCode.clear();
    pushBack(out_data,hdiffzData);
    return true;
}

bool serializeZipDiffData(std::vector<TByte>& out_data, UnZipper* newZip,UnZipper* oldZip,
                          const std::vector<uint32_t>& newReCompressList,
                          const std::vector<uint32_t>& samePairList,
                          const std::vector<uint32_t>& oldRefList,
                          const std::vector<TByte>&    hdiffzData,
                          hdiff_TCompress* compressPlugin){
    ZipDiffData  data;
    memset(&data,0,sizeof(ZipDiffData));
    data.newZipFileCount=UnZipper_fileCount(newZip);
    data.newZipIsNormalized=newZip->_isApkNormalized?1:0;
    data.newZipVCESize=newZip->_vce_size;
    data.reCompressList=(uint32_t*)newReCompressList.data();
    data.reCompressCount=newReCompressList.size();
    data.samePairList=(uint32_t*)samePairList.data();
    data.samePairCount=samePairList.size()/2;
    data.oldZipVCESize=oldZip->_vce_size;
    data.refList=(uint32_t*)oldRefList.data();
    data.refCount=oldRefList.size();
    data.oldCrc=OldStream_getOldCrc(oldZip,oldRefList.data(),oldRefList.size());
    return _serializeZipDiffData(out_data,&data,hdiffzData,compressPlugin);
}
