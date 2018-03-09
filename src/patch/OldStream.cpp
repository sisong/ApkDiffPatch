//  OldStream.cpp
//  ZipPatch
/*
 The MIT License (MIT)
 Copyright (c) 2016-2018 HouSisong
 
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
#include "patch.h"

static inline bool _refIsNeedDecomBuf(const UnZipper* zip,int fileIndex){
    return UnZipper_file_isCompressed(zip,fileIndex)
            && (!UnZipper_file_isApkV2Compressed(zip,fileIndex));
}

ZipFilePos_t OldStream_getDecompressSumSize(const UnZipper* oldZip,const uint32_t* refList,size_t refCount){
    ZipFilePos_t decompressSumSize=0;
    for (size_t i=0; i<refCount; ++i) {
        int fileIndex=(int)refList[i];
        if (_refIsNeedDecomBuf(oldZip,fileIndex)){
            decompressSumSize+=UnZipper_file_uncompressedSize(oldZip,fileIndex);
        }
    }
    return decompressSumSize;
}

bool OldStream_getDecompressData(UnZipper* oldZip,const uint32_t* refList,size_t refCount,
                                 hpatch_TStreamOutput* output_refStream){
    hpatch_StreamPos_t writeToPos=0;
    for (size_t i=0;i<refCount;++i){
        int fileIndex=(int)refList[i];
        if (_refIsNeedDecomBuf(oldZip,fileIndex)){
            if (!UnZipper_fileData_decompressTo(oldZip,fileIndex,output_refStream,writeToPos))
                return false;
            writeToPos+=UnZipper_file_uncompressedSize(oldZip,fileIndex);
        }
    }
    return true;
}

inline static void writeUInt32_to(unsigned char* out_buf4,uint32_t v){
    out_buf4[0]=(unsigned char)v; out_buf4[1]=(unsigned char)(v>>8);
    out_buf4[2]=(unsigned char)(v>>16); out_buf4[3]=(unsigned char)(v>>24);
}

uint32_t OldStream_getOldCrc(const UnZipper* oldZip,const uint32_t* refList,size_t refCount){
    unsigned char buf4[4];
    uLong crc=0;
    crc=crc32(crc,oldZip->_cache_vce,oldZip->_vce_size);
    for (size_t i=0;i<refCount;++i){
        int fileIndex=(int)refList[i];
        uint32_t fileCrc=UnZipper_file_crc32(oldZip,fileIndex);
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
                                 unsigned char* out_data,unsigned char* out_data_end){
    size_t curRangeIndex=self->_curRangeIndex;
    hpatch_StreamPos_t readPos=readFromPos - self->_rangeEndList[curRangeIndex-1]
                                + self->_rangeFileOffsets[curRangeIndex];
    if (curRangeIndex==0){
        memcpy(out_data,self->_oldZip->_cache_vce+readPos,out_data_end-out_data);
        return true;
    }else if (self->_rangIsInDecBuf[curRangeIndex]){
        long readLen=(long)(out_data_end-out_data);
        return (self->_input_decompressStream->read(self->_input_decompressStream->streamHandle,
                                                    readPos,out_data,out_data_end) == readLen);
    }else{
        return UnZipper_fileData_read(self->_oldZip,(ZipFilePos_t)readPos,out_data,out_data_end);
    }
}

static long _OldStream_read(hpatch_TStreamInputHandle streamHandle,
                            const hpatch_StreamPos_t _readFromPos,
                            unsigned char* out_data,unsigned char* out_data_end){
    OldStream* self=(OldStream*)streamHandle;
    const uint32_t* ranges=self->_rangeEndList;
    long  result=(long)(out_data_end-out_data);
    hpatch_StreamPos_t readFromPos=_readFromPos;
    while (out_data<out_data_end) {
        size_t curRangeIndex=self->_curRangeIndex;
        long readLen=(long)(out_data_end-out_data);
        if (ranges[curRangeIndex-1]<=readFromPos){ //-1 safe
            if (readFromPos+readLen<=ranges[curRangeIndex]){//hit all
                check(_OldStream_read_do(self,readFromPos,out_data,out_data_end));
                break; //ok out while
            }else if (readFromPos<=ranges[curRangeIndex]){//hit left
                long leftLen=(long)(ranges[curRangeIndex]-readFromPos);
                if (leftLen>0)
                    check(_OldStream_read_do(self,readFromPos,out_data,out_data+leftLen));
                ++self->_curRangeIndex;
                readFromPos+=leftLen;
                out_data+=leftLen;
                continue; //next
            }//else
        }//else
        
        //todo: optimize, binary search?
        self->_curRangeIndex=-1;
        for (size_t i=0; i<self->_rangeCount; ++i) {
            if (readFromPos<ranges[i]){
                self->_curRangeIndex=(int)i;
                break;
            }
        }
        check(self->_curRangeIndex>=0);
    }
clear:
    return result;
}

bool _createRange(OldStream* self,const uint32_t* refList,size_t refCount){
    bool result=true;
    assert(self->_buf==0);
    uint32_t curSumSize=0;
    uint32_t curDecompressPos=0;
    
    self->_rangeCount= 1 + refCount;
    self->_buf=(unsigned char*)malloc(sizeof(uint32_t)*self->_rangeCount*2+sizeof(uint32_t)+self->_rangeCount);
    check(self->_buf!=0);
    self->_rangeEndList=((uint32_t*)self->_buf)+1; //+1 for _rangeEndList[-1] safe
    self->_rangeFileOffsets=self->_rangeEndList+self->_rangeCount;
    self->_rangIsInDecBuf=(unsigned char*)(self->_rangeFileOffsets+self->_rangeCount);
    
    curSumSize=self->_oldZip->_vce_size;
    self->_rangeEndList[-1]=0;
    self->_rangeEndList[0]=curSumSize;
    self->_rangeFileOffsets[0]=0;
    self->_rangIsInDecBuf[0]=0;
    for (size_t i=0; i<refCount; ++i) {
        int fileIndex=(int)refList[i];
        bool isInDecBuf=_refIsNeedDecomBuf(self->_oldZip,fileIndex);
        self->_rangIsInDecBuf[i+1]=isInDecBuf?1:0;
        if (isInDecBuf){
            self->_rangeFileOffsets[i+1]=curDecompressPos;
            ZipFilePos_t uncompressedSize=UnZipper_file_uncompressedSize(self->_oldZip,fileIndex);
            curDecompressPos+=uncompressedSize;
            curSumSize+=uncompressedSize;
        }else{
            self->_rangeFileOffsets[i+1]=UnZipper_fileData_offset(self->_oldZip,fileIndex);
            ZipFilePos_t compressedSize=UnZipper_file_compressedSize(self->_oldZip,fileIndex);
            curSumSize+=compressedSize;
        }
        self->_rangeEndList[i+1]=curSumSize;
    }
    self->_curRangeIndex=0;
clear:
    return result;
}

bool OldStream_open(OldStream* self,UnZipper* oldZip,const uint32_t* refList,
                    size_t refCount,const hpatch_TStreamInput* input_decompressStream){
    bool result=true;
    uint32_t oldDataSize;
    check(self->stream==0);
    
    self->_oldZip=oldZip;
    self->_input_decompressStream=input_decompressStream;
    check(_createRange(self,refList,refCount));
    assert(self->_rangeCount>0);
    oldDataSize=self->_rangeEndList[self->_rangeCount-1];
    
    self->_stream.streamHandle=self;
    self->_stream.streamSize=oldDataSize;
    self->_stream.read=_OldStream_read;
    self->stream=&self->_stream;
clear:
    return result;
}
