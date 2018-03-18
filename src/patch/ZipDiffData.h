//  ZipDiffData.h
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
    size_t      newZipIsDataNormalized;
    size_t      newZipAlignSize;
    size_t      newCompressLevel;
    size_t      newCompressMemLevel;
    size_t      isEnableExtraEdit;
    size_t      newZipCESize;
    size_t      newZipVCESize;
    uint32_t*   samePairList;//[newFileIndex<->oldFileIndex,...]
    size_t      samePairCount;
    uint32_t*   newRefNotDecompressList;
    size_t      newRefNotDecompressCount;
    uint32_t*   newRefCompressedSizeList;//isCompressed && not in samePairList
    size_t      newRefCompressedSizeCount;
    size_t      oldZipIsDataNormalized;
    size_t      oldIsFileDataOffsetMatch;
    size_t      oldZipVCESize;
    uint32_t*   oldRefList;
    size_t      oldRefCount;
    uint32_t*   oldRefNotDecompressList;
    size_t      oldRefNotDecompressCount;
    uint32_t    oldCrc;
    const hpatch_TStreamInput* hdiffzData;
    const hpatch_TStreamInput* editV2Sign;
//private:
    TByte*              _buf;
    TFileStreamInput    _hdiffzData;
    TFileStreamInput    _editV2Sign;
} ZipDiffData;

void ZipDiffData_init(ZipDiffData* self);
bool ZipDiffData_isCanDecompress(TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin);
bool ZipDiffData_openRead(ZipDiffData* self,TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin);
void ZipDiffData_close(ZipDiffData* self);

#define  kExtraEdit     "ZiPatExtraData"
#define  kExtraEditLen  14
//if (isEnableExtraEdit ON) zip diff out:[head+hdiffzData+ExtraData+SizeOf(ExtraData)4Byte+kExtraEdit]

#ifdef __cplusplus
}
#endif
#endif //ZipPatch_ZipDiffData_h
