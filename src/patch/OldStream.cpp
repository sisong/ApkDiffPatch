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

bool OldStream_getDecompressData(UnZipper* oldZip,const uint32_t* decompressList,size_t decompressCount,
                                 hpatch_TStreamOutput* output_refStream){
    for (size_t i=0;i<decompressCount;++i){
        if (!UnZipper_fileData_decompressTo(oldZip,decompressList[i],output_refStream))
            return false;
    }
    return true;
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
    
    
    
    return 0;
}

bool _createRange(OldStream* self){
    assert(self->_buf==0);
    int fileCount=UnZipper_fileCount(self->_oldZip);
    int uncCount=UnZipper_uncompressed_fileCount(self->_oldZip);
    self->_rangeCount=uncCount+2;
    self->_buf=(unsigned char*)malloc(sizeof(uint32_t)*self->_rangeCount*2);
    self->_rangeEndList=(uint32_t*)self->_buf;
    self->_rangeFileOffsets=self->_rangeEndList+self->_rangeCount;
    
    int rangeIndex=0;
    uint32_t curSize=self->_oldZip->_vce_size;
    self->_rangeEndList[rangeIndex]=curSize;
    self->_rangeFileOffsets[rangeIndex]=0;
    ++rangeIndex;
    curSize+=self->_input_refStream->streamSize;
    self->_rangeEndList[rangeIndex]=curSize;
    self->_rangeFileOffsets[rangeIndex]=0;
    ++rangeIndex;
    for (int i=0; i<fileCount; ++i) {
        if (UnZipper_file_isCompressed(self->_oldZip,i)) continue;
        assert(rangeIndex<self->_rangeCount);
        curSize+=UnZipper_file_uncompressedSize(self->_oldZip,i);
        self->_rangeEndList[rangeIndex]=curSize;
        self->_rangeFileOffsets[rangeIndex]=UnZipper_fileData_offset(self->_oldZip,i);
        ++rangeIndex;
    }
    assert(rangeIndex==self->_rangeCount);
    return true;
}

bool OldStream_open(OldStream* self,UnZipper* oldZip,
                    const hpatch_TStreamInput* input_refStream,hpatch_StreamPos_t oldDataSize){
    assert(self->stream==0);
    bool result=true;
    bool _isInClear=false;
    self->_oldZip=oldZip;
    self->_input_refStream=input_refStream;
    _createRange(self);
    check(self->_rangeEndList[self->_rangeCount-1]==oldDataSize);
    
    self->_stream.streamHandle=self;
    self->_stream.streamSize=oldDataSize;
    self->_stream.read=_OldStream_read;
    self->stream=&self->_stream;
clear:
    _isInClear=true;
    return result;
}
