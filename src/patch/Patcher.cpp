//  Patcher.cpp
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
#include "Patcher.h"
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch.h" //https://github.com/sisong/HDiffPatch
#include "Zipper.h"
#include "ZipDiffData.h"
#include "OldStream.h"
#include "NewStream.h"
#include "patch_types.h"

#define _CompressPlugin_lzma  //default support lzma
#include "../../lzma/C/LzmaDec.h" // http://www.7-zip.org/sdk.html
#define _CompressPlugin_lzma2 //default support lzma2
#include "../../lzma/C/Lzma2Dec.h"

#include "../../HDiffPatch/decompress_plugin_demo.h"

#define  check(value,error) { \
    if (!(value)){ printf(#value" "#error"!\n");  \
        if (result==PATCH_SUCCESS) result=error; if (!_isInClear){ goto clear; } } }

#if (!_IS_NEED_VIRTUAL_ZIP)
static
#endif
TPatchResult VirtualZipPatchWithStream(const hpatch_TStreamInput* oldZipStream,const hpatch_TStreamInput* zipDiffStream,
                                       const hpatch_TStreamOutput* outNewZipStream,size_t maxUncompressMemory,
                                       const char* tempUncompressFileName,int threadNum,
                                       IVirtualZip_in* _virtual_in,IVirtualZip_out* virtual_out){
#define HPATCH_CACHE_SIZE  (128*1024)
    UnZipper            oldZip;
    Zipper              out_newZip;
    ZipDiffData         zipDiffData;
    OldStream           oldStream;
    NewStream           newStream;
    hpatch_compressedDiffInfo   diffInfo;
    TByte*                      ref_cache=0;
    hpatch_TFileStreamOutput    io_refFile;
    hpatch_TStreamOutput        io_ref_cache;
    hpatch_TStreamInput*        input_ref=0;
    hpatch_TStreamOutput*       output_ref=0;
    TPatchResult    result=PATCH_SUCCESS;
    bool            _isInClear=false;
    bool            isUsedTempFile=false;
    TByte*          temp_cache =0;
    ZipFilePos_t    decompressSumSize=0;
    hpatch_TDecompress* decompressPlugin=0;

    UnZipper_init(&oldZip);
    Zipper_init(&out_newZip);
    ZipDiffData_init(&zipDiffData);
    OldStream_init(&oldStream);
    NewStream_init(&newStream);
    hpatch_TFileStreamOutput_init(&io_refFile);
    
#if (_IS_NEED_VIRTUAL_ZIP)
    VirtualZip_in*  virtual_in=0;
    VirtualZip_in   _virtual_in_;
    if (_virtual_in) { VirtualZip_in_init(&_virtual_in_); virtual_in=&_virtual_in_; }
#else
    assert(_virtual_in==0);
    assert(virtual_out==0);
#endif

#ifdef _CompressPlugin_lzma2
    if (decompressPlugin==0){
        if (ZipDiffData_isCanDecompress(zipDiffStream,&lzma2DecompressPlugin))
            decompressPlugin=&lzma2DecompressPlugin;
    }
#endif
#ifdef _CompressPlugin_lzma
    if (decompressPlugin==0){
        if (ZipDiffData_isCanDecompress(zipDiffStream,&lzmaDecompressPlugin))
            decompressPlugin=&lzmaDecompressPlugin;
    }
#endif
    if (decompressPlugin==0){
        decompressPlugin=&zlibDecompressPlugin;
        check(ZipDiffData_isCanDecompress(zipDiffStream,decompressPlugin),PATCH_COMPRESSTYPE_ERROR);
    }
    
    check(ZipDiffData_openRead(&zipDiffData,zipDiffStream,decompressPlugin),PATCH_ZIPDIFFINFO_ERROR);
    check(UnZipper_openStream(&oldZip,oldZipStream,zipDiffData.oldZipIsDataNormalized!=0,
                            zipDiffData.oldIsFileDataOffsetMatch!=0),PATCH_OPENREAD_ERROR);
    check(zipDiffData.oldZipCESize==UnZipper_CESize(&oldZip),PATCH_OLDDATA_ERROR);
#if (_IS_NEED_VIRTUAL_ZIP)
    if (virtual_in)
        check(VirtualZip_in_open(virtual_in,_virtual_in,&oldZip),PATCH_VIRTUAL_IN_BEGIN_ERROR);
#endif
    check(zipDiffData.oldCrc==OldStream_getOldCrc(&oldZip,zipDiffData.oldRefList,zipDiffData.oldRefCount
                                                  _VIRTUAL_IN(virtual_in)), PATCH_OLDDATA_ERROR);
    
    check(getCompressedDiffInfo(&diffInfo,zipDiffData.hdiffzData),PATCH_HDIFFINFO_ERROR);
    if (strlen(diffInfo.compressType) > 0)
        check(decompressPlugin->is_can_open(diffInfo.compressType),PATCH_COMPRESSTYPE_ERROR);
    
    decompressSumSize=OldStream_getDecompressSumSize(&oldZip,zipDiffData.oldRefList,zipDiffData.oldRefCount
                                                     _VIRTUAL_IN(virtual_in));
    
    isUsedTempFile=(decompressSumSize > maxUncompressMemory)&&(tempUncompressFileName!=0);
    if (isUsedTempFile){
        check(hpatch_TFileStreamOutput_open(&io_refFile,tempUncompressFileName,decompressSumSize),
              PATCH_OPENWRITE_ERROR);
        output_ref=&io_refFile.base;
        input_ref=(hpatch_TStreamInput*)&io_refFile.base;
    }else{
        ref_cache=(TByte*)malloc(decompressSumSize+1);//+1 not null
        check(ref_cache!=0, PATCH_MEM_ERROR);
        mem_as_hStreamOutput(&io_ref_cache,ref_cache,ref_cache+decompressSumSize);
        output_ref=&io_ref_cache;
        input_ref=(hpatch_TStreamInput*)&io_ref_cache;
    }
    
    check(OldStream_getDecompressData(&oldZip,zipDiffData.oldRefList,zipDiffData.oldRefCount,
                                      output_ref _VIRTUAL_IN(virtual_in)),PATCH_OLDDECOMPRESS_ERROR);
    check(OldStream_open(&oldStream,&oldZip,zipDiffData.oldRefList,zipDiffData.oldRefCount,
                         0,0,input_ref _VIRTUAL_IN(virtual_in)), PATCH_OLDSTREAM_ERROR);
    check(oldStream.stream->streamSize==diffInfo.oldDataSize,PATCH_OLDDATA_ERROR);
    
    check(Zipper_openStream(&out_newZip,outNewZipStream,(int)zipDiffData.newZipFileCount,
                          (int)zipDiffData.newZipAlignSize,(int)zipDiffData.newCompressLevel,
                          (int)zipDiffData.newCompressMemLevel),PATCH_OPENWRITE_ERROR);
    check(NewStream_open(&newStream,&out_newZip,&oldZip,  (size_t)diffInfo.newDataSize,
                         zipDiffData.newZipIsDataNormalized!=0,
                         zipDiffData.newZipCESize,zipDiffData.extraEdit,
                         zipDiffData.samePairList,zipDiffData.samePairCount,
                         zipDiffData.newRefOtherCompressedList,zipDiffData.newRefOtherCompressedCount,
                         (int)zipDiffData.newOtherCompressLevel,(int)zipDiffData.newOtherCompressMemLevel,
                         zipDiffData.newRefCompressedSizeList,zipDiffData.newRefCompressedSizeCount,
                         threadNum _VIRTUAL_IN(virtual_in) _VIRTUAL_OUT(virtual_out)),PATCH_NEWSTREAM_ERROR);
    
    temp_cache =(TByte*)malloc(HPATCH_CACHE_SIZE);
    check(temp_cache!=0,PATCH_MEM_ERROR);
    check(patch_decompress_with_cache(newStream.stream,oldStream.stream,zipDiffData.hdiffzData,
                                      decompressPlugin,temp_cache,temp_cache+HPATCH_CACHE_SIZE),PATCH_HPATCH_ERROR);
    check(newStream.isFinish,PATCH_ZIPPATCH_ERROR);
    
clear:
    _isInClear=true;
    check(Zipper_close(&out_newZip),PATCH_CLOSEFILE_ERROR);
    NewStream_close(&newStream);
    OldStream_close(&oldStream);
#if (_IS_NEED_VIRTUAL_ZIP)
    if (virtual_in) check(VirtualZip_in_close(virtual_in),PATCH_VIRTUAL_IN_END_ERROR);
#endif
    check(UnZipper_close(&oldZip),PATCH_CLOSEFILE_ERROR);
    ZipDiffData_close(&zipDiffData);
    check(hpatch_TFileStreamOutput_close(&io_refFile),PATCH_CLOSEFILE_ERROR);
    if (isUsedTempFile) hpatch_removeFile(tempUncompressFileName);
    if (temp_cache) free(temp_cache);
    if (ref_cache) free(ref_cache);
    return result;
}

TPatchResult ZipPatchWithStream(const hpatch_TStreamInput* oldZipStream,const hpatch_TStreamInput* zipDiffStream,
                                const hpatch_TStreamOutput* outNewZipStream,
                                size_t maxUncompressMemory,const char* tempUncompressFileName,int threadNum){
    return VirtualZipPatchWithStream(oldZipStream,zipDiffStream,outNewZipStream,
                                     maxUncompressMemory,tempUncompressFileName,threadNum,0,0);
}

#if (!_IS_NEED_VIRTUAL_ZIP)
static
#endif
TPatchResult VirtualZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath,
                             size_t maxUncompressMemory,const char* tempUncompressFileName,int threadNum,
                             IVirtualZip_in* virtual_in,IVirtualZip_out* virtual_out){
    hpatch_TFileStreamInput    oldZipStream;
    hpatch_TFileStreamInput    zipDiffStream;
    hpatch_TFileStreamOutput   outNewZipStream;
    TPatchResult    result=PATCH_SUCCESS;
    bool            _isInClear=false;
    
    hpatch_TFileStreamInput_init(&zipDiffStream);
    hpatch_TFileStreamInput_init(&oldZipStream);
    hpatch_TFileStreamOutput_init(&outNewZipStream);
    check(hpatch_TFileStreamInput_open(&oldZipStream,oldZipPath),PATCH_OPENREAD_ERROR);
    check(hpatch_TFileStreamInput_open(&zipDiffStream,zipDiffPath),PATCH_OPENREAD_ERROR);
    check(hpatch_TFileStreamOutput_open(&outNewZipStream,outNewZipPath,(hpatch_StreamPos_t)(-1)),PATCH_OPENWRITE_ERROR);
    hpatch_TFileStreamOutput_setRandomOut(&outNewZipStream,hpatch_TRUE);
    result=VirtualZipPatchWithStream(&oldZipStream.base,&zipDiffStream.base,&outNewZipStream.base,
                                     maxUncompressMemory,tempUncompressFileName,threadNum,virtual_in,virtual_out);
clear:
    _isInClear=true;
    check(hpatch_TFileStreamOutput_close(&outNewZipStream),PATCH_CLOSEFILE_ERROR);
    check(hpatch_TFileStreamInput_close(&oldZipStream),PATCH_CLOSEFILE_ERROR);
    check(hpatch_TFileStreamInput_close(&zipDiffStream),PATCH_CLOSEFILE_ERROR);
    return result;
}

TPatchResult ZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath,
                      size_t maxUncompressMemory,const char* tempUncompressFileName,int threadNum){
    return VirtualZipPatch(oldZipPath,zipDiffPath,outNewZipPath,
                           maxUncompressMemory,tempUncompressFileName,threadNum,0,0);
}
