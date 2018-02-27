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
#ifdef __cplusplus
extern "C" {
#endif
#define kZipAlignSize 8
typedef uint32_t ZipFilePos_t;

typedef struct{
    ZipFilePos_t  endCDir_pos;
    int           fileHeader_count;
    ZipFilePos_t  fileHeader_size;
    ZipFilePos_t  fileHeader_pos;
} _t_EndCentralDirectoryInfo;

typedef struct UnZipper{
    FILE*           _file;
    ZipFilePos_t    _fileLength;
    ZipFilePos_t    _file_curPos;
    _t_EndCentralDirectoryInfo _endCDirInfo;
    ZipFilePos_t*   _fileHeaderOffsets;
    //mem
    unsigned char*  _buf;
    unsigned char*  _cache_fileHeader;
} UnZipper;
void UnZipper_init(UnZipper* self);
bool UnZipper_openRead(UnZipper* self,const char* zipFileName);
bool UnZipper_close(UnZipper* self);
inline static int   UnZipper_fileCount(const UnZipper* self) { return self->_endCDirInfo.fileHeader_count; }
int                  UnZipper_file_nameLen(const UnZipper* self,int fileIndex);
const unsigned char* UnZipper_file_nameBegin(const UnZipper* self,int fileIndex);
bool                UnZipper_file_isCompressed(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_file_compressedSize(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_file_uncompressedSize(const UnZipper* self,int fileIndex);
    
ZipFilePos_t        UnZipper_fileData_offset(UnZipper* self,int fileIndex);
bool                UnZipper_fileData_read(UnZipper* self,ZipFilePos_t file_pos,unsigned char* buf,unsigned char* bufEnd);
typedef bool (*UnZipper_fileData_callback)(void* dstHandle,const unsigned char* data,const  unsigned char* dataEnd);
bool                UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,
                                             void* dstHandle,UnZipper_fileData_callback callback);
bool                UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,
                                                   void* dstHandle,UnZipper_fileData_callback callback);

typedef struct Zipper{
    FILE*           _file;
    ZipFilePos_t    _curFilePos;
    int             _fileEntryMaxCount;
    int             _fileEntryCount;
    int             _fileHeaderCount;
    ZipFilePos_t*   _fileEntryOffsets;
    unsigned char*  _extFieldLens;
    //mem
    unsigned char*  _buf;
    size_t          _curBufLen;
} Zipper;
void Zipper_init(Zipper* self);
bool Zipper_openWrite(Zipper* self,const char* zipFileName,int fileEntryMaxCount);
bool Zipper_close(Zipper* self);
bool Zipper_file_append(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_file_appendWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                 const unsigned char* data,size_t dataSize,bool isNeedCompress);
bool Zipper_fileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_endCentralDirectory_append(Zipper* self,UnZipper* srcZip);
#ifdef __cplusplus
}
#endif
#endif /* Zipper_h */
