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
#include "../patch/patch_types.h"
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

static bool getNormalizedCompressedCode(UnZipper* selfZip,int selfIndex,
                                        std::vector<TByte>& out_compressedCode,int zlibCompressLevel){
    size_t selfFileSize=UnZipper_file_uncompressedSize(selfZip,selfIndex);
    size_t maxCodeSize=Zipper_compressData_maxCodeSize(selfFileSize);
    std::vector<TByte>& buf(out_compressedCode);
    buf.resize(selfFileSize+maxCodeSize);
    hpatch_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,buf.data(),buf.data()+selfFileSize);
    check(UnZipper_fileData_decompressTo(selfZip,selfIndex,&stream));
    size_t compressedSize=Zipper_compressData(buf.data(),selfFileSize,buf.data()+selfFileSize,
                                              maxCodeSize,zlibCompressLevel);
    buf.resize(compressedSize);
    return compressedSize>0;
}


static bool _getZipIsCompressedBy(UnZipper* zip,int zlibCompressLevel){
    std::vector<TByte> oldCompressedCode;
    std::vector<TByte> newCompressedCode;
    int fileCount=UnZipper_fileCount(zip);
    for (int i=0; i<fileCount; ++i) {
        if (UnZipper_file_isCompressed(zip,i)){
            if (UnZipper_file_isApkV2Compressed(zip,i)) continue;
            size_t compressedSize=UnZipper_file_compressedSize(zip,i);
            check(getNormalizedCompressedCode(zip,i,newCompressedCode,zlibCompressLevel));
            if (compressedSize!=newCompressedCode.size()) return false;
            
            ZipFilePos_t pos=UnZipper_fileData_offset(zip,i);
            oldCompressedCode.resize(compressedSize);
            check(UnZipper_fileData_read(zip,pos,oldCompressedCode.data(),oldCompressedCode.data()+compressedSize));
            if (0==memcmp(newCompressedCode.data(),oldCompressedCode.data(),compressedSize))
                return false;
        }
    }
    return true;
}

bool getZipCompressedDataIsNormalized(UnZipper* zip,int* out_zlibCompressLevel){
    if (_getZipIsCompressedBy(zip,kDefaultZlibCompressLevel))
        { *out_zlibCompressLevel=kDefaultZlibCompressLevel; return true; }
    for (int i=Z_BEST_SPEED;i<=Z_BEST_COMPRESSION;++i){
        if (i==kDefaultZlibCompressLevel) continue;
        if (_getZipIsCompressedBy(zip,i))
            { *out_zlibCompressLevel=i; return true; }
    }
    return false;
}


static bool _isAligned(const std::vector<ZipFilePos_t>& offsetList,ZipFilePos_t alignSize){
    for (size_t i=0; i<offsetList.size(); ++i) {
        if ((offsetList[i]%alignSize)!=0)
            return false;
    }
    return true;
}
size_t getZipAlignSize_unsafe(UnZipper* zip){
    int fileCount=UnZipper_fileCount(zip);
    ZipFilePos_t maxSkipLen=0;
    ZipFilePos_t minOffset=1024*4; //set search max AlignSize
    std::vector<ZipFilePos_t> offsetList;
    for (int i=0; i<fileCount; ++i){
        bool isNeedAlign=(!UnZipper_file_isCompressed(zip,i))
                        &&(UnZipper_file_compressedSize(zip,i)>0);
        if (!isNeedAlign)
            continue;
        ZipFilePos_t lastEndPos=(i>0)?(UnZipper_fileData_offset(zip,i-1) //unsafe 可能并没有按顺序放置?
                                       +UnZipper_file_compressedSize(zip,i-1)) : 0;
        ZipFilePos_t entryOffset=UnZipper_fileEntry_offset_unsafe(zip,i);
        if ((entryOffset-lastEndPos>=12)&&(i>0)){//可能上一个file有Data descriptor块;
            //尝试修正lastEndPos;
            uint32_t crc=UnZipper_file_crc32(zip,i-1);
            uint32_t compressedSize=UnZipper_file_compressedSize(zip,i-1);
            TByte buf[16];
            check(UnZipper_fileData_read(zip,lastEndPos,buf,buf+16));
            if ((readUInt32(buf)==crc)&&(readUInt32(buf+4)==compressedSize))
                lastEndPos+=12;
            else if ((readUInt32(buf+4)==crc)&&(readUInt32(buf+8)==compressedSize))
                lastEndPos+=16;
        }
        ZipFilePos_t skipLen=entryOffset-lastEndPos;
        if (skipLen>maxSkipLen) maxSkipLen=skipLen;
        ZipFilePos_t offset=UnZipper_fileData_offset(zip,i);
        if (offset<minOffset) minOffset=offset;
        offsetList.push_back(offset);
    }
    if (offsetList.empty())
        return 1;
    for (ZipFilePos_t align=minOffset; align>=maxSkipLen+1; --align) {
        if (_isAligned(offsetList,align))
            return align;
    }
    return 0;
}

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,int zlibCompressLevel,
                     std::vector<uint32_t>& out_samePairList,
                     std::vector<uint32_t>& out_newRefList,
                     std::vector<uint32_t>& out_newRefNotDecompressList,
                     std::vector<uint32_t>& out_newRefCompressedSizeList){
    int oldFileCount=UnZipper_fileCount(oldZip);
    typedef std::multimap<uint32_t,int> TMap;
    TMap crcMap;
    for (int i=0; i<oldFileCount; ++i) {
        uint32_t crcOld=UnZipper_file_crc32(oldZip,i);
        crcMap.insert(TMap::value_type(crcOld,i));
    }
    
    int newFileCount=UnZipper_fileCount(newZip);
    for (int i=0; i<newFileCount; ++i) {
        if (UnZipper_file_isApkV2Compressed(newZip,i)){
            out_newRefNotDecompressList.push_back(i);
            out_newRefCompressedSizeList.push_back(UnZipper_file_compressedSize(newZip,i));
        }else{
            bool findSame=false;
            int  oldSameIndex=-1;
            uint32_t crcNew=UnZipper_file_crc32(newZip,i);
            std::pair<TMap::const_iterator,TMap::const_iterator> range=crcMap.equal_range(crcNew);
            for (;range.first!=range.second;++range.first) {
                int oldIndex=range.first->second;
                if (zipFileData_isSame(newZip,i,oldZip,oldIndex)){
                    if (UnZipper_file_isApkV2Compressed(oldZip,oldIndex))
                        continue;
                    findSame=true;
                    oldSameIndex=oldIndex;
                    break;
                }else{
                    printf("WARNING: crc32 equal but data not equal! file index: %d,%d\n",i,oldIndex);
                    printf("   name:\"%s\"\n        \"%s\"\n",zipFile_name(newZip,i).c_str(),
                           zipFile_name(oldZip,oldIndex).c_str());
                }
            }
            if (findSame){
                out_samePairList.push_back(i);
                out_samePairList.push_back(oldSameIndex);
            }else{
                out_newRefList.push_back(i);
                if (UnZipper_file_isCompressed(newZip,i)){
                    std::vector<TByte> code;
                    check(getNormalizedCompressedCode(newZip,i,code,zlibCompressLevel));
                    out_newRefCompressedSizeList.push_back((uint32_t)code.size());
                }
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

bool readZipStreamData(UnZipper* zip,const std::vector<uint32_t>& refList,
                       const std::vector<uint32_t>& refNotDecompressList,std::vector<unsigned char>& out_data){
    size_t outSize=0;
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
    check(OldStream_open(&stream,zip,refList.data(),refList.size(),
                         refNotDecompressList.data(),refNotDecompressList.size(),&in_decompressStream));
    
    outSize=(size_t)stream.stream->streamSize;
    assert(outSize==stream.stream->streamSize);
    out_data.resize(outSize);
    check(outSize==(size_t)stream.stream->read(stream.stream->streamHandle,0,out_data.data(),out_data.data()+outSize));
    return true;
}


static void pushIncList(std::vector<TByte>& out_data,const uint32_t* list,size_t count){
    uint32_t backValue=~(uint32_t)0;
    for (size_t i=0;i<count;++i){
        uint32_t curValue=list[i];
        packUInt(out_data,(uint32_t)(curValue-(uint32_t)(backValue+1)));
        backValue=curValue;
    }
}

static bool _serializeZipDiffData(std::vector<TByte>& out_data,const ZipDiffData*  data,
                                  const std::vector<TByte>& hdiffzData,hdiff_TCompress* compressPlugin){
    std::vector<TByte> headData;
    {//head data
        uint32_t backPairNew=~(uint32_t)0;
        uint32_t backPairOld=~(uint32_t)0;
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
        pushIncList(headData,data->newRefNotDecompressList,data->newRefNotDecompressCount);
        for (size_t i=0;i<data->newRefCompressedSizeCount;++i){
            packUInt(headData,(uint32_t)data->newRefCompressedSizeList[i]);
        }
        pushIncList(headData,data->oldRefList,data->oldRefCount);
        pushIncList(headData,data->oldRefNotDecompressList,data->oldRefNotDecompressCount);
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
        #define kVersionTypeLen 7
        static const char* kVersionType="ZiPat1&";
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
    packUInt(out_data,data->newZipIsDataNormalized);
    packUInt(out_data,data->newZipAlignSize);
    packUInt(out_data,data->newZipVCESize);
    packUInt(out_data,data->samePairCount);
    packUInt(out_data,data->newRefNotDecompressCount);
    packUInt(out_data,data->newRefCompressedSizeCount);
    packUInt(out_data,data->compressLevel);
    packUInt(out_data,data->oldZipIsDataNormalized);
    packUInt(out_data,data->oldIsFileDataOffsetMatch);
    packUInt(out_data,data->oldZipVCESize);
    packUInt(out_data,data->oldRefCount);
    packUInt(out_data,data->oldRefNotDecompressCount);
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
                          size_t newZipAlignSize,size_t compressLevel,
                          const std::vector<uint32_t>& samePairList,
                          const std::vector<uint32_t>& newRefNotDecompressList,
                          const std::vector<uint32_t>& newRefCompressedSizeList,
                          const std::vector<uint32_t>& oldRefList,
                          const std::vector<uint32_t>& oldRefNotDecompressList,
                          const std::vector<TByte>&    hdiffzData,
                          hdiff_TCompress* compressPlugin){
    ZipDiffData  data;
    memset(&data,0,sizeof(ZipDiffData));
    data.newZipFileCount=UnZipper_fileCount(newZip);
    data.newZipIsDataNormalized=newZip->_isDataNormalized?1:0;
    data.newZipAlignSize=newZipAlignSize;
    data.newZipVCESize=newZip->_vce_size;
    data.samePairList=(uint32_t*)samePairList.data();
    data.samePairCount=samePairList.size()/2;
    data.newRefNotDecompressList=(uint32_t*)newRefNotDecompressList.data();
    data.newRefNotDecompressCount=newRefNotDecompressList.size();
    data.newRefCompressedSizeList=(uint32_t*)newRefCompressedSizeList.data();
    data.newRefCompressedSizeCount=newRefCompressedSizeList.size();
    data.compressLevel=compressLevel;
    data.oldZipIsDataNormalized=(UnZipper_isHaveApkV2Sign(oldZip)&&oldZip->_isDataNormalized)?1:0;
    data.oldIsFileDataOffsetMatch=(UnZipper_isHaveApkV2Sign(oldZip)&&oldZip->_isFileDataOffsetMatch)?1:0;
    data.oldZipVCESize=oldZip->_vce_size;
    data.oldRefList=(uint32_t*)oldRefList.data();
    data.oldRefCount=oldRefList.size();
    data.oldRefNotDecompressList=(uint32_t*)oldRefNotDecompressList.data();
    data.oldRefNotDecompressCount=oldRefNotDecompressList.size();
    data.oldCrc=OldStream_getOldCrc(oldZip,oldRefList.data(),oldRefList.size(),
        oldRefNotDecompressList.data(),oldRefNotDecompressList.size());
    return _serializeZipDiffData(out_data,&data,hdiffzData,compressPlugin);
}
