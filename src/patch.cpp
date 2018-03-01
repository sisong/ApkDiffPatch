//  patch.cpp
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
#include "patch.h"
#include <stdio.h>
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch.h"
#include "../../HDiffPatch/file_for_patch.h"

#include "../../zstd/lib/zstd.h"
#define _IsNeedIncludeDefaultCompressHead 0
#define _CompressPlugin_zstd
#include "../../HDiffPatch/decompress_plugin_demo.h"

#include "patch/Zipper.h"

#define  check(value,error) { \
    if (!(value)){ printf(#value" "#error"!\n");  \
        if (result!=PATCH_SUCCESS) result=error; if (!_isInClear){ goto clear; } } }


TPatchResult ZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath,
                      size_t maxUncompressMemory,const char* tempUncompressFileName){
    #define CACHE_SIZE  (1<<22)
    UnZipper            oldZip;
    TFileStreamInput    diffData;
    Zipper              newZip;
    TPatchResult    result=PATCH_SUCCESS;
    bool            _isInClear=false;
    bool            isUsedTempFile=false;
    TByte*          temp_cache =0;
    
    hpatch_TDecompress* decompressPlugin=&zstdDecompressPlugin;;
    
    UnZipper_init(&oldZip);
    TFileStreamInput_init(&diffData);
    Zipper_init(&newZip);
    
    check(TFileStreamInput_open(&diffData,zipDiffPath),PATCH_READ_ERROR);
    check(UnZipper_openRead(&oldZip,oldZipPath),PATCH_READ_ERROR);
    /*{
        hpatch_compressedDiffInfo diffInfo;
        check(getCompressedDiffInfo(&diffInfo,&diffData.base),PATCH_HEADINFO_ERROR);
        check(diffInfo.oldDataSize == oldData.base.streamSize,PATCH_OLDDATA_ERROR);
        if (strlen(diffInfo.compressType) > 0) {
            check(decompressPlugin->is_can_open(decompressPlugin,&diffInfo),PATCH_COMPRESSTYPE_ERROR);
        }
        
        temp_cache = (TByte*)malloc(CACHE_SIZE);
        check(temp_cache!=0,PATCH_MEM_ERROR);
        check(TFileStreamOutput_open(&newData,outNewZipPath,diffInfo.newDataSize),PATCH_WRITE_ERROR);
        check(patch_decompress_with_cache(&newData.base, &oldData.base, &diffData.base,decompressPlugin,
                                          temp_cache,temp_cache+CACHE_SIZE),PATCH_ERROR);
    }*/
    
clear:
    _isInClear=true;
    check(Zipper_close(&newZip),PATCH_CLOSEFILE_ERROR);
    check(TFileStreamInput_close(&diffData),PATCH_CLOSEFILE_ERROR);
    check(UnZipper_close(&oldZip),PATCH_CLOSEFILE_ERROR);
    if (isUsedTempFile) remove(tempUncompressFileName);
    if (temp_cache) free(temp_cache);
    return result;
}
