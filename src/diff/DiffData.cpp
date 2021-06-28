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
#include <string>
#include <map>
#include "../patch/patch_types.h"
#include "../patch/OldStream.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/private_diff/pack_uint.h"
using namespace hdiff_private;

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }
#define  check_clear(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; assert(false); if (!_isInClear){ goto clear; } } }

#define  test_clear(value) { \
    if (!(value)){ \
        result=false; if (!_isInClear){ goto clear; } } }

bool zipFileData_isSame(UnZipper* selfZip,int selfIndex,UnZipper* srcZip,int srcIndex){
    uint32_t selfFileSize=UnZipper_file_uncompressedSize(selfZip,selfIndex);
    if (selfFileSize!=UnZipper_file_uncompressedSize(srcZip,srcIndex)) return false;
    if (selfFileSize==0) return true;
    std::vector<TByte> buf(selfFileSize*2);
    hpatch_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,buf.data(),buf.data()+selfFileSize);
    check(UnZipper_fileData_decompressTo(selfZip,selfIndex,&stream));
    mem_as_hStreamOutput(&stream,buf.data()+selfFileSize,buf.data()+selfFileSize*2);
    check(UnZipper_fileData_decompressTo(srcZip,srcIndex,&stream));
    return 0==memcmp(buf.data(),buf.data()+selfFileSize,selfFileSize);
}


bool getZipIsSameWithStream(const hpatch_TStreamInput* oldZipStream,
                            const hpatch_TStreamInput* newZipStream,bool* out_isOldHaveApkV2Sign){
    UnZipper            newZip;
    UnZipper            oldZip;
    std::map<std::string,int> oldMap;
    bool            result=true;
    bool            _isInClear=false;
    int             fileCount=0;
    
    UnZipper_init(&oldZip);
    UnZipper_init(&newZip);
    check_clear(UnZipper_openStream(&oldZip,oldZipStream));
    check_clear(UnZipper_openStream(&newZip,newZipStream));
    
    if (out_isOldHaveApkV2Sign){
        *out_isOldHaveApkV2Sign=UnZipper_isHaveApkV2Sign(&oldZip);
    }
    
    fileCount=UnZipper_fileCount(&oldZip);
    test_clear(fileCount==UnZipper_fileCount(&newZip));
    for (int i=0;i<fileCount; ++i)
        oldMap[zipFile_name(&oldZip,i)]=i;
    test_clear(oldMap.size()==(size_t)fileCount);
    
    for (int i=0;i<fileCount; ++i) {
        std::map<std::string,int>::iterator it=oldMap.find(zipFile_name(&newZip,i));
        test_clear(it!=oldMap.end());
        int old_i=it->second;
        oldMap.erase(it);//del
        test_clear(UnZipper_file_crc32(&oldZip,old_i)==UnZipper_file_crc32(&newZip,i));
        test_clear(zipFileData_isSame(&oldZip,old_i,&newZip,i));
    }
clear:
    _isInClear=true;
    check_clear(UnZipper_close(&newZip));
    check_clear(UnZipper_close(&oldZip));
    return result;}

bool getZipIsSame(const char* oldZipPath,const char* newZipPath,bool* out_isOldHaveApkV2Sign){
    hpatch_TFileStreamInput    oldZipStream;
    hpatch_TFileStreamInput    newZipStream;
    bool            result=true;
    bool            _isInClear=false;
    
    hpatch_TFileStreamInput_init(&oldZipStream);
    hpatch_TFileStreamInput_init(&newZipStream);
    check_clear(hpatch_TFileStreamInput_open(&oldZipStream,oldZipPath));
    check_clear(hpatch_TFileStreamInput_open(&newZipStream,newZipPath));
    result=getZipIsSameWithStream(&oldZipStream.base,&newZipStream.base);
clear:
    _isInClear=true;
    check_clear(hpatch_TFileStreamInput_close(&newZipStream));
    check_clear(hpatch_TFileStreamInput_close(&oldZipStream));
    return result;
}

static bool getNormalizedCompressedCode(UnZipper* selfZip,int selfIndex,
                                        std::vector<TByte>& out_compressedCode,
                                        int zlibCompressLevel,int zlibCompressMemLevel){
    size_t selfFileSize=UnZipper_file_uncompressedSize(selfZip,selfIndex);
    size_t maxCodeSize=Zipper_compressData_maxCodeSize(selfFileSize);
    std::vector<TByte>& buf(out_compressedCode);
    buf.resize(maxCodeSize+selfFileSize);
    hpatch_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,buf.data()+maxCodeSize,buf.data()+maxCodeSize+selfFileSize);
    check(UnZipper_fileData_decompressTo(selfZip,selfIndex,&stream));
    size_t compressedSize=Zipper_compressData(buf.data()+maxCodeSize,selfFileSize,buf.data(),
                                              maxCodeSize,zlibCompressLevel,zlibCompressMemLevel);
    buf.resize(compressedSize);
    return compressedSize>0;
}


bool getCompressedIsNormalizedBy(UnZipper* zip,int zlibCompressLevel,int zlibCompressMemLevel,bool testReCompressedByApkV2Sign){
    std::vector<TByte> oldCompressedCode;
    std::vector<TByte> newCompressedCode;
    int fileCount=UnZipper_fileCount(zip);
    for (int i=0; i<fileCount; ++i) {
        if (!UnZipper_file_isCompressed(zip,i)) continue;
        if (testReCompressedByApkV2Sign ^ UnZipper_file_isReCompressedByApkV2Sign(zip,i)) continue;
        check(getNormalizedCompressedCode(zip,i,newCompressedCode,zlibCompressLevel,zlibCompressMemLevel));
        size_t compressedSize=UnZipper_file_compressedSize(zip,i);
        if (compressedSize!=newCompressedCode.size()) return false;
        
        ZipFilePos_t pos=UnZipper_fileData_offset(zip,i);
        oldCompressedCode.resize(compressedSize);
        check(UnZipper_fileData_read(zip,pos,oldCompressedCode.data(),oldCompressedCode.data()+compressedSize));
        if (0!=memcmp(newCompressedCode.data(),oldCompressedCode.data(),compressedSize))
            return false;
    }
    return true;
}

bool getCompressedIsNormalized(UnZipper* zip,int* out_zlibCompressLevel,
                               int* out_zlibCompressMemLevel,bool testReCompressedByApkV2Sign){
    if (getCompressedIsNormalizedBy(zip,kDefaultZlibCompressLevel,kDefaultZlibCompressMemLevel,testReCompressedByApkV2Sign)){
        *out_zlibCompressLevel=kDefaultZlibCompressLevel;
        *out_zlibCompressMemLevel=kDefaultZlibCompressMemLevel;
        return true;
    }
    if (getCompressedIsNormalizedBy(zip,Z_BEST_COMPRESSION,kDefaultZlibCompressMemLevel,testReCompressedByApkV2Sign)){
        *out_zlibCompressLevel=Z_BEST_COMPRESSION;
        *out_zlibCompressMemLevel=kDefaultZlibCompressMemLevel;
        return true;
    }
    for (int ml=MAX_MEM_LEVEL;ml>=1;--ml){
        for (int cl=Z_BEST_SPEED;cl<=Z_BEST_COMPRESSION;++cl){
            if ((cl==kDefaultZlibCompressLevel)&(ml==kDefaultZlibCompressMemLevel)) continue;
            if ((cl==Z_BEST_COMPRESSION)&(ml==kDefaultZlibCompressMemLevel)) continue;
            if (getCompressedIsNormalizedBy(zip,cl,ml,testReCompressedByApkV2Sign)){
                *out_zlibCompressLevel=cl;
                *out_zlibCompressMemLevel=ml;
                return true;
            }
        }
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
    //note: 该函数对没有Normalized的zip允许获取AlignSize失败;
    int fileCount=UnZipper_fileCount(zip);
    ZipFilePos_t maxSkipLen=0;
    ZipFilePos_t minOffset=1024*4; //set search max AlignSize
    std::vector<ZipFilePos_t> offsetList;
    for (int i=0; i<fileCount; ++i){
        bool isNeedAlign= (!UnZipper_file_isCompressed(zip,i));
        if (!isNeedAlign)
            continue;
        ZipFilePos_t entryOffset=UnZipper_fileEntry_offset_unsafe(zip,i);
        ZipFilePos_t lastEndPos=(i<=0)?0:UnZipper_fileEntry_endOffset(zip,i-1); //unsafe last可能并没有按顺序放置?
        if (entryOffset<lastEndPos) return 0; //顺序有误;
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

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,
                     bool newCompressedDataIsNormalized,
                     int zlibCompressLevel,int zlibCompressMemLevel,
                     std::vector<uint32_t>& out_samePairList,
                     std::vector<uint32_t>& out_newRefList,
                     std::vector<uint32_t>& out_newRefOtherCompressedList,
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
        ZipFilePos_t compressedSize=UnZipper_file_compressedSize(newZip,i);
        if (UnZipper_file_isReCompressedByApkV2Sign(newZip,i)){
            out_newRefOtherCompressedList.push_back(i);
            out_newRefCompressedSizeList.push_back(compressedSize);
        }else if ((0==UnZipper_file_uncompressedSize(newZip,i))
                  &&(!UnZipper_file_isCompressed(newZip,i))){ //empty entry
            check(0==compressedSize);
            //not need: out_newRefList.push_back(i);
        }else{
            bool findSame=false;
            int  oldSameIndex=-1;
            uint32_t crcNew=UnZipper_file_crc32(newZip,i);
            std::pair<TMap::const_iterator,TMap::const_iterator> range=crcMap.equal_range(crcNew);
            for (;range.first!=range.second;++range.first) {
                int oldIndex=range.first->second;
                if (zipFileData_isSame(newZip,i,oldZip,oldIndex)){
                    if (UnZipper_file_isReCompressedByApkV2Sign(oldZip,oldIndex))
                        continue;
                    if ((!UnZipper_file_isCompressed(oldZip,oldIndex))&&
                        UnZipper_file_isCompressed(newZip,i)) continue;
                    findSame=true;
                    oldSameIndex=oldIndex;
                    if (zipFile_name(newZip,i)==zipFile_name(oldZip,oldIndex)) break;
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
                    if (newCompressedDataIsNormalized){
                        out_newRefCompressedSizeList.push_back(compressedSize);
                    }else{
                        std::vector<TByte> code;
                        check(getNormalizedCompressedCode(newZip,i,code,zlibCompressLevel,zlibCompressMemLevel));
                        out_newRefCompressedSizeList.push_back((uint32_t)code.size());
                    }
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
                       const std::vector<uint32_t>& refNotDecompressList,
                       std::vector<unsigned char>& out_data){
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
    check(OldStream_open(&stream,zip,refList.data(),refList.size(),refNotDecompressList.data(),
                         refNotDecompressList.size(),&in_decompressStream));
    
    outSize=(size_t)stream.stream->streamSize;
    assert(outSize==stream.stream->streamSize);
    out_data.resize(outSize);
    check(stream.stream->read(stream.stream,0,out_data.data(),out_data.data()+outSize));
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
                                  const std::vector<TByte>& hdiffzData,
                                  const hdiff_TCompress* compressPlugin,const UnZipper* newZip){
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
        pushIncList(headData,data->newRefOtherCompressedList,data->newRefOtherCompressedCount);
        for (size_t i=0;i<data->newRefCompressedSizeCount;++i){
            packUInt(headData,(uint32_t)data->newRefCompressedSizeList[i]);
        }
        pushIncList(headData,data->oldRefList,data->oldRefCount);
    }
    std::vector<TByte> headCode;
    {
        if (headData.empty()){
            headCode.clear();
        }else{
            headCode.resize((size_t)compressPlugin->maxCompressedSize(headData.size()));
            size_t codeSize=hdiff_compress_mem(compressPlugin,headCode.data(),headCode.data()+headCode.size(),
                                               headData.data(),headData.data()+headData.size());
            if ((0<codeSize)&(codeSize<=headCode.size()))
                headCode.resize(codeSize);
            else
                return false;//error
        }
    }
    
    {//type version
        static const char* kVersionType="ZiPat1&";
        pushBack(out_data,(const TByte*)kVersionType,(const TByte*)kVersionType+strlen(kVersionType));
    }
    {//compressType
        const char* compressType=compressPlugin->compressType();
        size_t compressTypeLen=strlen(compressType);
        if (compressTypeLen>hpatch_kMaxPluginTypeLength) return false;
        pushBack(out_data,(const TByte*)compressType,(const TByte*)compressType+compressTypeLen+1); //'\0'
    }
    //head info
    packUInt(out_data,data->PatchModel);
    packUInt(out_data,data->newZipFileCount);
    packUInt(out_data,data->newZipIsDataNormalized);
    packUInt(out_data,data->newZipAlignSize);
    packUInt(out_data,data->newCompressLevel);
    packUInt(out_data,data->newCompressMemLevel);
    packUInt(out_data,data->newOtherCompressLevel);
    packUInt(out_data,data->newOtherCompressMemLevel);
    packUInt(out_data,data->newZipCESize);
    packUInt(out_data,data->samePairCount);
    packUInt(out_data,data->newRefOtherCompressedCount);
    packUInt(out_data,data->newRefCompressedSizeCount);
    packUInt(out_data,data->oldZipIsDataNormalized);
    packUInt(out_data,data->oldIsFileDataOffsetMatch);
    packUInt(out_data,data->oldZipCESize);
    packUInt(out_data,data->oldRefCount);
    packUInt(out_data,data->oldCrc);
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    packUInt(out_data,hdiffzData.size());
    
    //code data
    headData.clear();
    pushBack(out_data,headCode);
    headCode.clear();
    pushBack(out_data,hdiffzData);
    
    {//ExtraEdit
        pushBack(out_data,newZip->_cache_fvce,newZip->_centralDirectory);
        uint32_t extraSize=(uint32_t)(newZip->_centralDirectory-newZip->_cache_fvce);
        TByte buf4[4];
        writeUInt32_to(buf4,extraSize);
        pushBack(out_data,buf4,buf4+4);
        pushBack(out_data,(const TByte*)kExtraEdit,(const TByte*)kExtraEdit+kExtraEditLen);
    }
    return true;
}

bool serializeZipDiffData(std::vector<TByte>& out_data, UnZipper* newZip,UnZipper* oldZip,
                          size_t newZipAlignSize,size_t compressLevel,size_t compressMemLevel,
                          size_t otherCompressLevel,size_t otherCompressMemLevel,
                          const std::vector<uint32_t>& samePairList,
                          const std::vector<uint32_t>& newRefOtherCompressedList,
                          const std::vector<uint32_t>& newRefCompressedSizeList,
                          const std::vector<uint32_t>& oldRefList,
                          const std::vector<TByte>&    hdiffzData,
                          const hdiff_TCompress* compressPlugin){
    ZipDiffData  data;
    memset(&data,0,sizeof(ZipDiffData));
    data.PatchModel=0; //now must 0
    data.newZipFileCount=UnZipper_fileCount(newZip);
    data.newZipIsDataNormalized=newZip->_isDataNormalized?1:0;
    data.newZipAlignSize=newZipAlignSize;
    data.newCompressLevel=compressLevel;
    data.newCompressMemLevel=compressMemLevel;
    data.newOtherCompressLevel=otherCompressLevel;
    data.newOtherCompressMemLevel=otherCompressMemLevel;
    data.newZipCESize=UnZipper_CESize(newZip);
    data.samePairList=(uint32_t*)samePairList.data();
    data.samePairCount=samePairList.size()/2;
    data.newRefOtherCompressedList=(uint32_t*)newRefOtherCompressedList.data();
    data.newRefOtherCompressedCount=newRefOtherCompressedList.size();
    data.newRefCompressedSizeList=(uint32_t*)newRefCompressedSizeList.data();
    data.newRefCompressedSizeCount=newRefCompressedSizeList.size();
    data.oldZipIsDataNormalized=(UnZipper_isHaveApkV2Sign(oldZip)&&oldZip->_isDataNormalized)?1:0;
    data.oldIsFileDataOffsetMatch=(UnZipper_isHaveApkV2Sign(oldZip)&&oldZip->_isFileDataOffsetMatch)?1:0;
    data.oldZipCESize=UnZipper_CESize(oldZip);
    data.oldRefList=(uint32_t*)oldRefList.data();
    data.oldRefCount=oldRefList.size();
    data.oldCrc=OldStream_getOldCrc(oldZip,oldRefList.data(),oldRefList.size());
    return _serializeZipDiffData(out_data,&data,hdiffzData,compressPlugin,newZip);
}
