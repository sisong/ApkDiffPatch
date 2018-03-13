//  Patcher.h
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
#ifndef ZipPatch_Patcher_h
#define ZipPatch_Patcher_h
#include <string.h>

typedef enum TPatchResult {
    PATCH_SUCCESS=0,
    PATCH_ERROR,
    PATCH_READ_ERROR,
    PATCH_WRITE_ERROR,
    PATCH_ZIPDIFFINFO_ERROR,
    PATCH_HDIFFINFO_ERROR,
    PATCH_COMPRESSTYPE_ERROR,
    PATCH_OLDDATA_ERROR,
    PATCH_NEWDATA_ERROR,
    PATCH_MEM_ERROR,
    PATCH_CLOSEFILE_ERROR
} TPatchResult;

TPatchResult ZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath,
                      size_t maxUncompressMemory,const char* tempUncompressFileName);

#endif //ZipPatch_Patcher_h
