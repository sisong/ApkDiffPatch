//  NewStream.cpp
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
#include "NewStream.h"

void NewStream_init(NewStream* self){
    memset(self,0,sizeof(NewStream));
}
void NewStream_close(NewStream* self){
    //todo:
}

static long _NewStream_write(hpatch_TStreamOutputHandle streamHandle,
                             const hpatch_StreamPos_t writeToPos,
                             const unsigned char* data,const unsigned char* data_end);

bool NewStream_open(NewStream* self,Zipper* out_newZip,UnZipper* oldZip,
                    size_t newDataSize,size_t newZipVCESize,
                    const uint32_t* samePairList,size_t samePairCount){
    self->_out_newZip=out_newZip;
    self->_oldZip=oldZip;
    self->_newZipVCESize=newZipVCESize;
    self->_samePairList=samePairList;
    self->_samePairCount=samePairCount;
    
    self->_stream.streamHandle=self;
    self->_stream.streamSize=newDataSize;
    self->_stream.write=_NewStream_write;
    self->stream=&self->_stream;
    
    return true;
}


#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=0; assert(false); goto clear; } }

static long _NewStream_write(hpatch_TStreamOutputHandle streamHandle,
                             const hpatch_StreamPos_t writeToPos,
                             const unsigned char* data,const unsigned char* data_end){
    long result=(long)(data_end-data);
    NewStream* self=(NewStream*)streamHandle;
    
    
clear:
    return result;
}
