//  DiffData.h
//  ZipDiff
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
#ifndef ZipDiff_DiffData_h
#define ZipDiff_DiffData_h
#include <vector>
#include <string>
#include "../patch/Zipper.h"
#include "../patch/ZipDiffData.h"
#include "../patch/patch_types.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff_types.h"
#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

#define  kDefaultZlibCompressLevel      6 // 1--9, for patch speed;
#define  kDefaultZipAlignSize           8 // 4,8,... for app speed;
#define  kDefaultZlibCompressMemLevel   DEF_MEM_LEVEL
#define  kDefaultDiffMatchScore         3 // for hdiff

bool zipFileData_isSame(UnZipper* self,int selfIndex,UnZipper* srcZip,int srcIndex);//byte by byte test
bool getZipIsSame(const char* oldZipPath,const char* newZipPath,bool* out_isOldHaveApkV2Sign=0);
bool getZipIsSameWithStream(const hpatch_TStreamInput* oldZipStream,
                            const hpatch_TStreamInput* newZipStream,bool* out_isOldHaveApkV2Sign=0);
bool getCompressedIsNormalized(UnZipper* zip,int* out_zlibCompressLevel,
                                      int* out_zlibCompressMemLevel,bool testReCompressedByApkV2Sign=false); //只检查压缩数据是否标准化;
bool getCompressedIsNormalizedBy(UnZipper* zip,int zlibCompressLevel,
                                      int zlibCompressMemLevel,bool testReCompressedByApkV2Sign=false); //只检查压缩数据是否标准化;
size_t getZipAlignSize_unsafe(UnZipper* zip); //只检查未压缩数据的起始位置对齐值,返回对齐值,0表示未对齐;

static inline std::string zipFile_name(UnZipper* self,int fileIndex){
    int nameLen=UnZipper_file_nameLen(self,fileIndex);
    const char* nameBegin=UnZipper_file_nameBegin(self,fileIndex);
    return std::string(nameBegin,nameBegin+nameLen);
}

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,
                     bool newCompressedDataIsNormalized,
                     int zlibCompressLevel,int zlibCompressMemLevel,
                     std::vector<uint32_t>& out_samePairList,
                     std::vector<uint32_t>& out_newRefList,
                     std::vector<uint32_t>& out_newRefOtherCompressedList,
                     std::vector<uint32_t>& out_newRefCompressedSizeList);

bool readZipStreamData(UnZipper* zip,const std::vector<uint32_t>& refList,
                       const std::vector<uint32_t>& refNotDecompressList,
                       std::vector<unsigned char>& out_data);

bool serializeZipDiffData(std::vector<TByte>& out_data, UnZipper* newZip,UnZipper* oldZip,
                          size_t newZipAlignSize,size_t compressLevel,size_t compressMemLevel,
                          size_t otherCompressLevel,size_t otherCompressMemLevel,
                          const std::vector<uint32_t>& samePairList,
                          const std::vector<uint32_t>& newRefOtherCompressedList,
                          const std::vector<uint32_t>& newRefCompressedSizeList,
                          const std::vector<uint32_t>& oldRefList,
                          const std::vector<TByte>&    hdiffzData,
                          const hdiff_TCompress*       compressPlugin);

#endif //ZipDiff_DiffData_h
