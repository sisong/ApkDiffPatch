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
#include "../../zlib1.2.11/zlib.h" //crc32 // http://zlib.net/  https://github.com/madler/zlib


ZipFilePos_t OldStream_getDecompressSumSize(const UnZipper* oldZip,const uint32_t* refList,size_t refCount){
    ZipFilePos_t decompressSumSize=0;
    for (int i=0; i<refCount; ++i) {
        int fileIndex=(int)refList[i];
        if (UnZipper_file_isCompressed(oldZip,fileIndex)){
            decompressSumSize+=UnZipper_file_uncompressedSize(oldZip,fileIndex);
        }
    }
    return decompressSumSize;
}

bool OldStream_getDecompressData(UnZipper* oldZip,const uint32_t* refList,size_t refCount,
                                 hpatch_TStreamOutput* output_refStream){
    for (int i=0;i<refCount;++i){
        int fileIndex=(int)refList[i];
        if (UnZipper_file_isCompressed(oldZip,fileIndex)){
            if (!UnZipper_fileData_decompressTo(oldZip,fileIndex,output_refStream))
                return false;
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
    for (int i=0;i<refCount;++i){
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
    //todo:
}

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; assert(false); if (!_isInClear){ goto clear; } } }


static long _OldStream_read(hpatch_TStreamInputHandle streamHandle,
                            const hpatch_StreamPos_t readFromPos,
                            unsigned char* out_data,unsigned char* out_data_end){
    OldStream* self=(OldStream*)streamHandle;
    
    long  result=(long)(out_data_end-out_data);
    if (readFromPos+result<=self->_rangeEndList[0]){
        //todo:
    }else{
        
    }
    
    
    return result;
}

bool _createRange(OldStream* self,const uint32_t* refList,size_t refCount){
    assert(self->_buf==0);
    self->_rangeCount= 1 + refCount;
    self->_buf=(unsigned char*)malloc((sizeof(uint32_t)*2+1)*self->_rangeCount);
    self->_rangeEndList=(uint32_t*)self->_buf;
    self->_rangeFileOffsets=self->_rangeEndList+self->_rangeCount;
    self->_rangIsCompressed=(unsigned char*)(self->_rangeFileOffsets+self->_rangeCount);
    
    uint32_t curSize=self->_oldZip->_vce_size;
    self->_rangeEndList[0]=curSize;
    self->_rangeFileOffsets[0]=0;
    self->_rangIsCompressed[0]=0;
    uint32_t curDecompressSize=0;
    for (int i=0; i<refCount; ++i) {
        int fileIndex=(int)refList[i];
        bool isCompressed=UnZipper_file_isCompressed(self->_oldZip,fileIndex);
        ZipFilePos_t uncompressedSize=UnZipper_file_uncompressedSize(self->_oldZip,fileIndex);
        curSize+=uncompressedSize;
        self->_rangeEndList[i+1]=curSize;
        self->_rangIsCompressed[i+1]=isCompressed?1:0;
        if (isCompressed) {
            self->_rangeFileOffsets[i+1]=curDecompressSize;
            curDecompressSize+=uncompressedSize;
        }else{
            self->_rangeFileOffsets[i+1]=UnZipper_fileData_offset(self->_oldZip,fileIndex);
        }
    }
    self->_curRangeIndex=0;
    return true;
}

bool OldStream_open(OldStream* self,UnZipper* oldZip,const uint32_t* refList,
                    size_t refCount,const hpatch_TStreamInput* input_decompressStream){
    assert(self->stream==0);
    bool result=true;
    bool _isInClear=false;
    self->_oldZip=oldZip;
    self->_input_decompressStream=input_decompressStream;
    _createRange(self,refList,refCount);
    assert(self->_rangeCount>0);
    uint32_t oldDataSize=self->_rangeEndList[self->_rangeCount-1];
    
    self->_stream.streamHandle=self;
    self->_stream.streamSize=oldDataSize;
    self->_stream.read=_OldStream_read;
    self->stream=&self->_stream;
clear:
    _isInClear=true;
    return result;
}
