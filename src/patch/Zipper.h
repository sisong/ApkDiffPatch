//
//  Zipper.h
//  ApkPatch
//
//  Created by housisong on 2018/2/25.
//  Copyright © 2018年 housisong. All rights reserved.
//

#ifndef Zipper_h
#define Zipper_h
#include <stdio.h> //FILE
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch_types.h"
//todo: support zip64

typedef unsigned long ZipFilePos_t;

struct _t_EndCentralDirectoryInfo{
    ZipFilePos_t  endCDir_pos;
    int           fileHeader_count;
    ZipFilePos_t  fileHeader_size;
    ZipFilePos_t  fileHeader_pos;
};

struct UnZipper{
    FILE*           _file;
    ZipFilePos_t    _fileLength;
    _t_EndCentralDirectoryInfo _endCDirInfo;
    unsigned char*  _buf;
    unsigned char*  _cache_fileHeader;
    int32_t*        _fileHeaderOffsets;
};
void UnZipper_init(UnZipper* self);
bool UnZipper_openRead(UnZipper* self,const char* zipFileName);
bool UnZipper_close(UnZipper* self);
inline static int   UnZipper_FileEntry_count(const UnZipper* self) { return self->_endCDirInfo.fileHeader_count; }
bool                UnZipper_FileEntry_isCompressed(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_FileEntry_compressedSize(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_FileEntry_uncompressedSize(const UnZipper* self,int fileIndex);
bool                UnZipper_FileEntry_decompress(UnZipper* self,int fileIndex,unsigned char* dst);

struct Zipper{
    FILE*           _file;
    int             _fileEntryMaxCount;
};
void Zipper_init(Zipper* self);
bool Zipper_openWrite(Zipper* self,const char* zipFileName,int fileEntryMaxCount);
bool Zipper_close(Zipper* self);
bool Zipper_FileEntry_append(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_FileEntry_appendWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                 const unsigned char* data,size_t dataSize,bool isNeedCompress);
bool Zipper_FileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_EndCentralDirectory_append(Zipper* self,UnZipper* srcZip);

#endif /* Zipper_h */
