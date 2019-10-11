//  OldStream.cpp
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
#include "OldStream.h"
#include <stdlib.h>
#include "patch_types.h"


int OldStream_getDecompressFileCount(const UnZipper* oldZip,const uint32_t* refList,
                                     size_t refCount _VIRTUAL_IN_D(virtual_in)){
    int result=0;
    for (size_t i=0; i<refCount; ++i){
        int fileIndex=(int)refList[i];
#if (_IS_NEED_VIRTUAL_ZIP)
        if ((virtual_in)&&(virtual_in->entryDatas[fileIndex])) {
            if (virtual_in->entryDatas[fileIndex]->isCompressed)
                ++result;
        } else
#endif
        if (UnZipper_file_isCompressed(oldZip,fileIndex))
            ++result;
    }
    return result;
}

ZipFilePos_t OldStream_getDecompressSumSize(const UnZipper* oldZip,const uint32_t* refList,
                                            size_t refCount _VIRTUAL_IN_D(virtual_in)){
    ZipFilePos_t decompressSumSize=0;
    for (size_t i=0; i<refCount; ++i) {
        int fileIndex=(int)refList[i];
#if (_IS_NEED_VIRTUAL_ZIP)
        if ((virtual_in)&&(virtual_in->entryDatas[fileIndex])) {
            if (virtual_in->entryDatas[fileIndex]->isCompressed)
                decompressSumSize+=virtual_in->entryDatas[fileIndex]->uncompressedSize;
        } else
#endif
            if (UnZipper_file_isCompressed(oldZip,fileIndex))
                decompressSumSize+=UnZipper_file_uncompressedSize(oldZip,fileIndex);
    }
    return decompressSumSize;
}

bool OldStream_getDecompressData(UnZipper* oldZip,const uint32_t* refList,size_t refCount,
                                 hpatch_TStreamOutput* output_refStream _VIRTUAL_IN_D(virtual_in)){
    hpatch_StreamPos_t writeToPos=0;
    for (size_t i=0;i<refCount;++i){
        int fileIndex=(int)refList[i];
#if (_IS_NEED_VIRTUAL_ZIP)
        if ((virtual_in)&&(virtual_in->entryDatas[fileIndex])) {
            TZipEntryData* entryData=virtual_in->entryDatas[fileIndex];
            if (entryData->isCompressed){
                if (!UnZipper_compressedData_decompressTo(oldZip,entryData->dataStream, 0,
                                                          entryData->dataStream->streamSize,
                                                          entryData->uncompressedSize,
                                                          output_refStream,writeToPos)) return false;
                writeToPos+=entryData->uncompressedSize;
            }
        } else
#endif
        if (UnZipper_file_isCompressed(oldZip,fileIndex)){
            if (!UnZipper_fileData_decompressTo(oldZip,fileIndex,output_refStream,writeToPos))
                return false;
            writeToPos+=UnZipper_file_uncompressedSize(oldZip,fileIndex);
        }
    }
    return true;
}

uint32_t OldStream_getOldCrc(const UnZipper* oldZip,const uint32_t* refList,
                             size_t refCount _VIRTUAL_IN_D(virtual_in)){
    unsigned char buf4[4];
    uLong crc=0;
    const unsigned char* CE;
#if (_IS_NEED_VIRTUAL_ZIP)
    if (virtual_in)
        CE=UnZipper_CEData(&virtual_in->virtualZip);
    else
#endif
        CE=UnZipper_CEData(oldZip);
    crc=crc32(crc,CE,(uInt)UnZipper_CESize(oldZip));
    for (size_t i=0;i<refCount;++i){
        int fileIndex=(int)refList[i];
        uint32_t fileCrc;
#if (_IS_NEED_VIRTUAL_ZIP)
        if ((virtual_in)&&(virtual_in->entryDatas[fileIndex]))
            fileCrc=UnZipper_file_crc32(&virtual_in->virtualZip,fileIndex);
        else
#endif
            fileCrc=UnZipper_file_crc32(oldZip,fileIndex);
        writeUInt32_to(buf4,fileCrc);
        crc=crc32(crc,buf4,4);
    }
    return (uint32_t)crc;
}


void OldStream_init(OldStream* self){
    memset(self,0,sizeof(OldStream));
}
void OldStream_close(OldStream* self){
    if (self->_buf) { free(self->_buf); self->_buf=0; }
}

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; assert(false); goto clear; } }


static bool _OldStream_read_do(OldStream* self,hpatch_StreamPos_t readFromPos,
                                 unsigned char* out_data,unsigned char* out_data_end,int curRangeIndex){
    hpatch_StreamPos_t readPos=readFromPos - self->_rangeEndList[curRangeIndex-1];
#if (_IS_NEED_VIRTUAL_ZIP)
    if ((self->_vin)&&(self->_rangIsInVirtualBuf[curRangeIndex])){
        int fileIndex=self->_rangeFileOffsets[curRangeIndex];
        TZipEntryData* entryData=self->_vin->entryDatas[fileIndex];
        return hpatch_FALSE!=entryData->dataStream->read(entryData->dataStream,
                                                         readPos,out_data,out_data_end);
    }
#endif
    readPos+=self->_rangeFileOffsets[curRangeIndex];
    if (curRangeIndex==0){
        const unsigned char* src;
#if (_IS_NEED_VIRTUAL_ZIP)
        if (self->_vin)
            src=UnZipper_CEData(&self->_vin->virtualZip);
        else
#endif
            src=UnZipper_CEData(self->_oldZip);
        memcpy(out_data,src+readPos,out_data_end-out_data);
        return true;
    }else if (self->_rangIsInDecBuf[curRangeIndex]){
        return hpatch_FALSE!=self->_input_decompressedStream->read(self->_input_decompressedStream,
                                                                   readPos,out_data,out_data_end);
    }else{
        return UnZipper_fileData_read(self->_oldZip,(ZipFilePos_t)readPos,out_data,out_data_end);
    }
}

static int findRangeIndex(const uint32_t* ranges,size_t rangeCount,uint32_t pos){
    //optimize, binary search?
    for (size_t i=0; i<rangeCount; ++i) {
        if (pos>=ranges[i]) continue;
        return (int)i;
    }
    return -1;
}

static hpatch_BOOL _OldStream_read(const hpatch_TStreamInput* stream,
                                   const hpatch_StreamPos_t _readFromPos,
                                   unsigned char* out_data,unsigned char* out_data_end){
    OldStream* self=(OldStream*)stream->streamImport;
    const uint32_t* ranges=self->_rangeEndList;
    int curRangeIndex=self->_curRangeIndex;
    hpatch_BOOL  result=hpatch_TRUE;
    size_t readFromPos=(size_t)_readFromPos;
    while (out_data<out_data_end) {
        size_t readLen=(size_t)(out_data_end-out_data);
        if (ranges[curRangeIndex-1]<=readFromPos){ //-1 safe
            if (readFromPos+readLen<=ranges[curRangeIndex]){//hit all
                check(_OldStream_read_do(self,readFromPos,out_data,out_data_end,curRangeIndex));
                break; //ok out while
            }else if (readFromPos<=ranges[curRangeIndex]){//hit left
                size_t leftLen=(size_t)(ranges[curRangeIndex]-readFromPos);
                if (leftLen>0)
                    check(_OldStream_read_do(self,readFromPos,out_data,out_data+leftLen,curRangeIndex));
                ++curRangeIndex;
                readFromPos+=leftLen;
                out_data+=leftLen;
                continue; //next
            }//else
        }//else
        
        curRangeIndex=findRangeIndex(ranges,self->_rangeCount,(uint32_t)readFromPos);
        check(curRangeIndex>=0);
    }
    self->_curRangeIndex=curRangeIndex;
clear:
    return result;
}

bool _createRange(OldStream* self,const uint32_t* refList,size_t refCount,
                  const uint32_t* refNotDecompressList,size_t refNotDecompressCount){
    bool result=true;
    assert(self->_buf==0);
    const int fileCount=UnZipper_fileCount(self->_oldZip);
    size_t   rangIndex;
    uint32_t curSumSize=0;
    uint32_t curDecompressPos=0;
    size_t refi=0;
    size_t noti=0;
    
    self->_rangeCount= 1 + refCount + refNotDecompressCount;
    size_t _memSize=sizeof(uint32_t)*(self->_rangeCount*2+1)+self->_rangeCount*sizeof(unsigned char);
#if (_IS_NEED_VIRTUAL_ZIP)
    if (self->_vin)
        _memSize+=self->_rangeCount*sizeof(unsigned char);
#endif
    self->_buf=(unsigned char*)malloc(_memSize);
    check(self->_buf!=0);
    self->_rangeEndList=((uint32_t*)self->_buf)+1; //+1 for _rangeEndList[-1] safe
    self->_rangeFileOffsets=self->_rangeEndList+self->_rangeCount;
    self->_rangIsInDecBuf=(unsigned char*)(self->_rangeFileOffsets+self->_rangeCount);
#if (_IS_NEED_VIRTUAL_ZIP)
    if (self->_vin){
        self->_rangIsInVirtualBuf=self->_rangIsInDecBuf+self->_rangeCount;
        memset(self->_rangIsInVirtualBuf,0,self->_rangeCount);
    }
#endif
    curSumSize=(uint32_t)UnZipper_CESize(self->_oldZip);
    self->_rangeEndList[-1]=0;
    self->_rangeEndList[0]=curSumSize;
    self->_rangeFileOffsets[0]=0;
    self->_rangIsInDecBuf[0]=0;
    rangIndex=1;
    for (int fileIndex=0; fileIndex<fileCount; ++fileIndex) {
        if ((refi<refCount)&&((int)refList[refi]==fileIndex)){
            ZipFilePos_t rangeSize;
            bool         isCompressed;
#if (_IS_NEED_VIRTUAL_ZIP)
            if ((self->_vin)&&(self->_vin->entryDatas[fileIndex])){
                TZipEntryData* entryData=self->_vin->entryDatas[fileIndex];
                rangeSize=entryData->uncompressedSize;
                isCompressed=entryData->isCompressed;
                if (!isCompressed) self->_rangIsInVirtualBuf[rangIndex]=1;
            } else
#endif
            {
                rangeSize=UnZipper_file_uncompressedSize(self->_oldZip,fileIndex);
                isCompressed=UnZipper_file_isCompressed(self->_oldZip,fileIndex);
            }
            if (isCompressed){
                self->_rangIsInDecBuf[rangIndex]=1;
                self->_rangeFileOffsets[rangIndex]=curDecompressPos;
                curDecompressPos+=rangeSize;
            }else{
                self->_rangIsInDecBuf[rangIndex]=0;
#if (_IS_NEED_VIRTUAL_ZIP)
                if ((self->_vin)&&(self->_rangIsInVirtualBuf[rangIndex]))
                    self->_rangeFileOffsets[rangIndex]=fileIndex;
                else
#endif
                    self->_rangeFileOffsets[rangIndex]=UnZipper_fileData_offset(self->_oldZip,fileIndex);
            }
            curSumSize+=rangeSize;
            self->_rangeEndList[rangIndex]=curSumSize;
            ++refi;
            ++rangIndex;
        }
        if ((noti<refNotDecompressCount)&&((int)refNotDecompressList[noti]==fileIndex)){
            ZipFilePos_t rangeSize=UnZipper_file_compressedSize(self->_oldZip,fileIndex);
            self->_rangIsInDecBuf[rangIndex]=0;
            self->_rangeFileOffsets[rangIndex]=UnZipper_fileData_offset(self->_oldZip,fileIndex);
            curSumSize+=rangeSize;
            self->_rangeEndList[rangIndex]=curSumSize;
            ++noti;
            ++rangIndex;
        }
    }
    assert(rangIndex==self->_rangeCount);
    assert(curDecompressPos==self->_input_decompressedStream->streamSize);
    self->_curRangeIndex=0;
clear:
    return result;
}

bool OldStream_open(OldStream* self,UnZipper* oldZip,const uint32_t* refList,size_t refCount,
                    const uint32_t* refNotDecompressList,size_t refNotDecompressCount,
                    const hpatch_TStreamInput* input_decompressedStream _VIRTUAL_IN_D(virtual_in)){
    bool result=true;
    uint32_t oldDataSize;
    check(self->stream==0);
    
    self->_oldZip=oldZip;
    self->_input_decompressedStream=input_decompressedStream;
#if (_IS_NEED_VIRTUAL_ZIP)
    self->_vin=virtual_in;
    if (virtual_in!=0)
        check(refNotDecompressCount==0);
#endif
    check(_createRange(self,refList,refCount,refNotDecompressList,refNotDecompressCount));
    assert(self->_rangeCount>0);
    oldDataSize=self->_rangeEndList[self->_rangeCount-1];
    
    self->_stream.streamImport=self;
    self->_stream.streamSize=oldDataSize;
    self->_stream.read=_OldStream_read;
    self->stream=&self->_stream;
clear:
    return result;
}
