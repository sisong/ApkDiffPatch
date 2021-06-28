//  Differ.h
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
#ifndef ZipDiff_Differ_h
#define ZipDiff_Differ_h
#include "../../HDiffPatch/file_for_patch.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff_types.h"


typedef enum TCheckZipDiffResult {
    CHECK_BYTE_BY_BYTE_EQUAL_TRUE=0, //ok
    CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_FALSE, //ok
    CHECK_SAME_LIKE_TRUE__BYTE_BY_BYTE_EQUAL_ERROR, //error
    CHECK_SAME_LIKE_ERROR,
    CHECK_ZIPPATCH_ERROR,
    CHECK_OTHER_ERROR
} TCheckZipDiffResult;

bool ZipDiff(const char* oldZipPath,const char* newZipPath,const char* outDiffFileName,
             const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
             int diffMatchScore,bool* out_isNewZipApkV2SignNoError=0);
bool ZipDiffWithStream(const hpatch_TStreamInput* oldZipStream,const hpatch_TStreamInput* newZipStream,
                       const hpatch_TStreamOutput* outDiffStream,
                       const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
                       int diffMatchScore,bool* out_isNewZipApkV2SignNoError=0);

TCheckZipDiffResult checkZipDiff(const char* oldZipPath,const char* newZipPath,const char* diffFileName,int threadNum);
TCheckZipDiffResult checkZipDiffWithStream(const hpatch_TStreamInput* oldZipStream,
                                           const hpatch_TStreamInput* newZipStream,
                                           const hpatch_TStreamInput* diffStream,int threadNum);

#endif //ZipDiff_Differ_h
