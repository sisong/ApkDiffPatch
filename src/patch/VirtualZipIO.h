//  VirtualZipIO.h
//  ZipPatch
/*
 The MIT License (MIT)
 Copyright (c) 2019 HouSisong
 
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

#ifndef VirtualZipIO_h
#define VirtualZipIO_h
#include "Zipper.h"

#if (_IS_NEED_VIRTUAL_ZIP)
//for Virtual Zip
struct IVirtualZip_in;
struct IVirtualZip_out;

typedef struct TZipEntryData {
    const hpatch_TStreamInput*    dataStream;
    ZipFilePos_t    uncompressedSize;
    bool            isCompressed;
} TZipEntryData;

typedef struct IVirtualZip_in{
    void*  virtualImport;
    //if no virtual, set out_entryDatas[i]=0; return false when error;
    bool (*beginVirtual)(IVirtualZip_in* self,const UnZipper* zip,TZipEntryData* out_entryDatas[]);
    uint32_t (*getCrc32)(const IVirtualZip_in* virtual_in,const UnZipper* zip,int fileIndex,
                         const TZipEntryData* entryData);
    bool   (*endVirtual)(IVirtualZip_in* self);
} IVirtualZip_in;

typedef enum TVirtualZip_out_type{
    kVirtualZip_out_error=0,
    kVirtualZip_out_void,                   // not need virtual out
    kVirtualZip_out_emptyFile_cast,         // 0fileSize in zip,not change crc; and virtual out but not need data
    kVirtualZip_out_emptyFile_uncompressed, // 0fileSize in zip,not change crc; and virtual out uncompressed data
    //other type now unsupport
} TVirtualZip_out_type;
typedef struct IVirtualZip_out{
    void*       virtualImport;
    hpatch_TStreamOutput* virtualStream; //stream for virtual out
    //if need out data, must open virtualStream
    TVirtualZip_out_type     (*beginVirtual)(IVirtualZip_out* self,const UnZipper* newInfoZip,int newFileIndex);
    TVirtualZip_out_type (*beginVirtualCopy)(IVirtualZip_out* self,const UnZipper* newInfoZip,int newFileIndex,
                                             const UnZipper* oldZip,int oldFileIndex);
    //when out data end, can close virtualStream and virtualStream==0
    bool                       (*endVirtual)(IVirtualZip_out* self);
} IVirtualZip_out;


struct VirtualZip_in{
    IVirtualZip_in* import;
    TZipEntryData** entryDatas;
    UnZipper        virtualZip;
};

inline static void VirtualZip_in_init(VirtualZip_in* vin){
    memset(vin,0,sizeof(VirtualZip_in)-sizeof(UnZipper));
    UnZipper_init(&vin->virtualZip);
}
inline static bool VirtualZip_in_open(VirtualZip_in* vin,IVirtualZip_in* import,UnZipper* oldZip){
    assert(vin->import==0);
    const int fCount=UnZipper_fileCount(oldZip);
    vin->entryDatas=(TZipEntryData**)malloc(sizeof(TZipEntryData*)*fCount);
    if (vin->entryDatas==0) return false;
    memset(vin->entryDatas,0,sizeof(TZipEntryData*)*fCount);
    vin->import=import;
    if (!import->beginVirtual(import,oldZip,vin->entryDatas)) return false;

    const ZipFilePos_t CESize=(ZipFilePos_t)UnZipper_CESize(oldZip);
    if (!UnZipper_openVirtualVCE(&vin->virtualZip,CESize,fCount)) return false;
    memcpy(vin->virtualZip._centralDirectory,oldZip->_centralDirectory,CESize);
    if (!UnZipper_updateVirtualVCE(&vin->virtualZip,oldZip->_isDataNormalized,CESize)) return false;
    for (int i=0; i<fCount; ++i) {
        TZipEntryData* entryData=vin->entryDatas[i];
        if (entryData==0) continue;
        uint32_t newCrc32=import->getCrc32(import,oldZip,i,entryData);
        UnZipper_updateVirtualFileInfo(&vin->virtualZip,i,entryData->uncompressedSize,
                                       (ZipFilePos_t)entryData->dataStream->streamSize,newCrc32);
    }
    return true;
}
inline static bool VirtualZip_in_close(VirtualZip_in* vin){
    if (vin==0) return true;
    bool result=UnZipper_close(&vin->virtualZip);
    if (vin->entryDatas){
        if (vin->import){
            if (!vin->import->endVirtual(vin->import))
                result=false;
        }
        free(vin->entryDatas);
        vin->entryDatas=0;
    }
    vin->import=0;
    return result;
}

#define _VIRTUAL_IN_D(p)    ,VirtualZip_in* p
#define _VIRTUAL_IN(p)      ,p
#define _VIRTUAL_OUT_D(p)   ,IVirtualZip_out* p
#define _VIRTUAL_OUT(p)     ,p

#else
#   define IVirtualZip_in   void
#   define IVirtualZip_out  void
#   define _VIRTUAL_IN_D(p)
#   define _VIRTUAL_IN(p)
#   define _VIRTUAL_OUT_D(p)
#   define _VIRTUAL_OUT(p)
#endif

#endif /* VirtualZipIO_h */
