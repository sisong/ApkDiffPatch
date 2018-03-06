//  Zipper.h
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
#ifndef ZipPatch_Zipper_h
#define ZipPatch_Zipper_h
#include <stdio.h> //FILE
#include <assert.h>
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch_types.h"
//todo: support zip64
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t ZipFilePos_t;

typedef struct UnZipper{
    hpatch_TStreamInput* stream;
//private:
    FILE*           _file;
    ZipFilePos_t    _fileLength;
    ZipFilePos_t    _file_curPos;
    hpatch_TStreamInput _stream;
    
    ZipFilePos_t    _vce_size;
    unsigned char*  _endCentralDirectory;
    unsigned char*  _centralDirectory;
    uint32_t*       _fileHeaderOffsets; //在_centralDirectory中的偏移位置;
    uint32_t*       _fileCompressedSizes;
    ZipFilePos_t*   _fileDataOffsets;
    //mem
    unsigned char*  _buf; //file read buf
    unsigned char*  _cache_vce;
} UnZipper;
void UnZipper_init(UnZipper* self);
bool UnZipper_openRead(UnZipper* self,const char* zipFileName);
bool UnZipper_close(UnZipper* self);
int                 UnZipper_fileCount(const UnZipper* self);
int                 UnZipper_file_nameLen(const UnZipper* self,int fileIndex);
const unsigned char* UnZipper_file_nameBegin(const UnZipper* self,int fileIndex);
bool                UnZipper_file_isCompressed(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_file_compressedSize(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_file_uncompressedSize(const UnZipper* self,int fileIndex);
uint32_t            UnZipper_file_crc32(const UnZipper* self,int fileIndex);
    
ZipFilePos_t        UnZipper_fileData_offset(UnZipper* self,int fileIndex);
bool                UnZipper_fileData_read(UnZipper* self,ZipFilePos_t file_pos,unsigned char* buf,unsigned char* bufEnd);
bool                UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,const hpatch_TStreamOutput* outStream);
bool                UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,const hpatch_TStreamOutput* outStream);
    
bool UnZipper_openForVCE(UnZipper* self,ZipFilePos_t vce_size,ZipFilePos_t vce_ESize,int fileCount);

    struct Zipper;
    struct _zlib_TCompress;
    struct Zipper_file_append_stream:public hpatch_TStreamOutput{
    //private:
        hpatch_StreamPos_t    inputPos;
        hpatch_StreamPos_t    outputPos;
        struct Zipper*        self;

        struct _zlib_TCompress* compressHandle;
        hpatch_TStreamOutput    compressOutStream;
        ZipFilePos_t            curFileIndex;
        static long _append_part_input(hpatch_TStreamOutputHandle handle,hpatch_StreamPos_t pos,
                                       const unsigned char* part_data,const unsigned char* part_data_end);
        static long _append_part_output(hpatch_TStreamOutputHandle handle,hpatch_StreamPos_t pos,
                                       const unsigned char* part_data,const unsigned char* part_data_end);
    };
    
typedef struct Zipper{
//private:
    FILE*           _file;
    ZipFilePos_t    _curFilePos;
    int             _fileEntryMaxCount;
    int             _fileEntryCount;
    int             _fileHeaderCount;
    ZipFilePos_t    _centralDirectory_pos;
    ZipFilePos_t*   _fileEntryOffsets;
    uint32_t*       _fileCompressedSizes;
    unsigned char*  _codeBuf;
    Zipper_file_append_stream _append_stream;
    //mem
    unsigned char*  _buf; //file out buf
    size_t          _curBufLen;
} Zipper;
void Zipper_init(Zipper* self);
bool Zipper_openWrite(Zipper* self,const char* zipFileName,int fileEntryMaxCount);
bool Zipper_close(Zipper* self);
bool Zipper_file_append_copy(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_file_append(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                        const unsigned char* data,size_t dataSize,bool dataIsCompressed);
bool Zipper_file_append_begin(Zipper* self,UnZipper* srcZip,int srcFileIndex,size_t dataSize,bool dataIsCompressed);
const hpatch_TStreamOutput* Zipper_file_append_part_as_stream(Zipper* self);
bool Zipper_file_append_part(Zipper* self,const unsigned char* part_data,size_t partSize);
bool Zipper_file_append_end(Zipper* self);
    
bool Zipper_fileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_endCentralDirectory_append(Zipper* self,UnZipper* srcZip);
#ifdef __cplusplus
}
#endif
#endif //ZipPatch_Zipper_h
