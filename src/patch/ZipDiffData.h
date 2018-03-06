//  ZipDiffData.h
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
#ifndef ZipPatch_ZipDiffData_h
#define ZipPatch_ZipDiffData_h
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch_types.h"
#include "../../HDiffPatch/file_for_patch.h"
#ifdef __cplusplus
extern "C" {
#endif
    
//解析补丁文件,获得diff元信息 和 HDiffZ数据(模拟成一个输入流);
typedef struct ZipDiffData{
    size_t      newZipFileCount;
    size_t      newZipVCESize;
    size_t      newZipVCE_ESize;
    size_t      reCompressCount;
    size_t      samePairCount;
    size_t      oldZipVCESize;
    size_t      refCount;
    size_t      refSumSize;
    uint32_t    oldCrc;
    uint32_t*   reCompressList;//isCompressed && not in samePairList
    uint32_t*   refList;  //used old data file
    uint32_t*   samePairList;//newFileIndex<->oldFileIndex
    const hpatch_TStreamInput* hdiffzData;
//private:
    TByte*              _buf;
    TFileStreamInput    _hdiffzData;
} ZipDiffData;

void ZipDiffData_init(ZipDiffData* self);
bool ZipDiffData_open(ZipDiffData* self,TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin);
void ZipDiffData_close(ZipDiffData* self);

#ifdef __cplusplus
}
#endif
#endif //ZipPatch_ZipDiffData_h
