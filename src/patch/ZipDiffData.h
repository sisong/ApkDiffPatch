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
#include "patch_types.h"
#include "../../HDiffPatch/file_for_patch.h"
#ifdef __cplusplus
extern "C" {
#endif

//解析补丁文件,获得diff元信息 和 HDiffZ数据(模拟成一个输入流);
typedef struct ZipDiffData{
    size_t      PatchModel;
    size_t      newZipFileCount;
    size_t      newZipIsDataNormalized;
    size_t      newZipAlignSize;
    size_t      newCompressLevel;
    size_t      newCompressMemLevel;
    size_t      newOtherCompressLevel;
    size_t      newOtherCompressMemLevel;
    size_t      newZipCESize;
    uint32_t*   samePairList;//[newFileIndex<->oldFileIndex,...]
    size_t      samePairCount;
    uint32_t*   newRefOtherCompressedList;
    size_t      newRefOtherCompressedCount;
    uint32_t*   newRefCompressedSizeList;//isCompressed && not in samePairList
    size_t      newRefCompressedSizeCount;
    size_t      oldZipIsDataNormalized;
    size_t      oldIsFileDataOffsetMatch;
    size_t      oldZipCESize;
    uint32_t*   oldRefList;
    size_t      oldRefCount;
    uint32_t    oldCrc;
    size_t      normalizeSoPageAlign;
    bool        pageAlignCompatible;
    const hpatch_TStreamInput* hdiffzData;
    const hpatch_TStreamInput* extraEdit;
//private:
    TByte*              _buf;
    TStreamInputClip    _hdiffzData;
    TStreamInputClip    _extraEdit;
} ZipDiffData;

void ZipDiffData_init(ZipDiffData* self);
bool ZipDiffData_isCanDecompress(const hpatch_TStreamInput* diffData,hpatch_TDecompress* decompressPlugin);
bool ZipDiffData_openRead(ZipDiffData* self,const hpatch_TStreamInput* diffData,
                          hpatch_TDecompress* decompressPlugin);
void ZipDiffData_close(ZipDiffData* self);

static const TByte kPackedEmptyPrefix=(1<<7)|0;
static const char* kExtraEdit = "ZiPat1&Extra";
#define            kExtraEditLen  12  // ==strlen(kExtraEdit)
//zip diff out:[head+hdiffzData+ ExtraData +SizeOf(ExtraData)4Byte+kExtraEdit]

#ifdef __cplusplus
}
#endif
#endif //ZipPatch_ZipDiffData_h
