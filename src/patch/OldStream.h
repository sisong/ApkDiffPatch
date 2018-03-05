//  OldStream.h
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
#ifndef ZipPatch_OldStream_h
#define ZipPatch_OldStream_h
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch_types.h"
#include "Zipper.h"

//利用oldZip、refList模拟成一个输入流;
typedef struct OldStream{
    const hpatch_TStreamInput* stream;
//private:
    UnZipper*                   _oldZip;
    const hpatch_TStreamInput*  _input_refStream;
    const uint32_t*             _oldZipNEFilePosList;
    size_t                      _oldZipNEFilePosCount;
    hpatch_TStreamInput         _stream;
    
    
    //mem
    unsigned char*              _buf;
} OldStream;


bool OldStream_getDecompressData(UnZipper* oldZip,const uint32_t* decompressList,
                                 size_t decompressCount, hpatch_TStreamOutput* output_refStream);
void OldStream_init(OldStream* self);
bool OldStream_open(OldStream* self,UnZipper* oldZip,const hpatch_TStreamInput* input_refStream,
                    hpatch_StreamPos_t oldDataSize,const uint32_t* oldZipNEFilePosList,size_t oldZipNEFilePosCount);
void OldStream_close(OldStream* self);

#endif //ZipPatch_OldStream_h
