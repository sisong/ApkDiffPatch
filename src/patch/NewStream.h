//  NewStream.h
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
#ifndef ZipPatch_NewStream_h
#define ZipPatch_NewStream_h
#include "patch_types.h"
#include "Zipper.h"
#include "VirtualZipIO.h"

//对外模拟成一个输出流;
//利用samePairList、CHEqs数据对需要输出的数据进行加工生成newZip;
typedef struct NewStream{
    const hpatch_TStreamOutput* stream;
    bool                    isFinish;
//private:
    Zipper*                 _out_newZip;
    UnZipper*               _oldZip;
    bool                    _newZipIsDataNormalized;
    size_t                  _newZipCESize;
    const uint32_t*         _samePairList;
    size_t                  _samePairCount;
    const uint32_t*         _newRefOtherCompressedList;
    size_t                  _newRefOtherCompressedCount;
    bool                    _newOtherCompressIsValid;
    int                     _newOtherCompressLevel;
    int                     _newOtherCompressMemLevel;
    const uint32_t*         _newReCompressSizeList;
    size_t                  _newReCompressSizeCount;
    hpatch_TStreamOutput    _stream;
    int                     _fileCount;
    size_t                  _extraEditSize;
    int                     _threadNum;
    
    int                     _curFileIndex;
    hpatch_StreamPos_t      _curWriteToPosEnd;
    size_t                  _curSamePairIndex;
    size_t                  _curNewOtherCompressIndex;
    size_t                  _curNewReCompressSizeIndex;
    bool                    _isAlwaysReCompress;
#if (_IS_NEED_VIRTUAL_ZIP)
    IVirtualZip_out*        _vout;
    VirtualZip_in*          _vin;
#endif

    UnZipper                _newZipVCE;
} NewStream;

void NewStream_init(NewStream* self);
bool NewStream_open(NewStream* self,Zipper* out_newZip,UnZipper* oldZip,
                    size_t newDataSize,bool newZipIsDataNormalized,
                    size_t newZipCESize,const hpatch_TStreamInput* extraEdit,
                    const uint32_t* samePairList,size_t samePairCount,
                    uint32_t* newRefOtherCompressedList,size_t newRefOtherCompressedCount,
                    int newOtherCompressLevel,int newOtherCompressMemLevel,
                    const uint32_t* reCompressList,size_t reCompressCount,
                    int threadNum=1 _VIRTUAL_IN_D(virtual_in=0) _VIRTUAL_OUT_D(virtual_out=0));
void NewStream_close(NewStream* self);

#endif //ZipPatch_NewStream_h
