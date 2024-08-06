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
#include "patch_types.h"
#include "../../HDiffPatch/file_for_patch.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t ZipFilePos_t;
    
// https://en.wikipedia.org/wiki/Zip_(file_format)
// https://source.android.com/security/apksigning/v2
// https://docs.oracle.com/javase/8/docs/technotes/guides/jar/jar.html#Signed_JAR_File

typedef struct UnZipper{
    const hpatch_TStreamInput* stream;
//private:
    hpatch_TFileStreamInput _fileStream;
    
    unsigned char*  _endCentralDirectory;
    unsigned char*  _centralDirectory;
    ZipFilePos_t*   _fileHeaderOffsets; //在_centralDirectory中的偏移位置;
    ZipFilePos_t*   _fileCompressedSizes;
    unsigned char*  _dataDescriptors;
    int             _dataDescriptorCount;
    ZipFilePos_t*   _fileDataOffsets;
    ZipFilePos_t*   _fileEntryOffsets;
    bool            _isSaved_fileEntryOffsets;
    bool            _isDataNormalized;
    bool            _isFileDataOffsetMatch;
    bool            _isHaveV3Sign;
    //mem
    unsigned char*  _buf; //file read buf
    unsigned char*  _vce;
    unsigned char*  _cache_fvce;// fileDataEnd_fillZero + v2signBlock + centralDirectory + endCentralDirectory
    ZipFilePos_t    _fvce_size;
} UnZipper;
void UnZipper_init(UnZipper* self);
bool UnZipper_openFile(UnZipper* self,const char* zipFileName,
                       bool isDataNormalized=false,bool isFileDataOffsetMatch=false,bool isSaved_fileEntryOffsets=false);
bool UnZipper_openStream(UnZipper* self,const hpatch_TStreamInput* zipStream,
                         bool isDataNormalized=false,bool isFileDataOffsetMatch=false,bool isSaved_fileEntryOffsets=false);
bool UnZipper_close(UnZipper* self);
int                 UnZipper_fileCount(const UnZipper* self);
int                 UnZipper_file_nameLen(const UnZipper* self,int fileIndex);
const char*         UnZipper_file_nameBegin(const UnZipper* self,int fileIndex);
bool                UnZipper_file_isCompressed(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_file_uncompressedSize(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_file_compressedSize(const UnZipper* self,int fileIndex);
uint32_t            UnZipper_file_crc32(const UnZipper* self,int fileIndex);
ZipFilePos_t        UnZipper_fileEntry_endOffset(const UnZipper* self,int fileIndex);


ZipFilePos_t        UnZipper_fileData_offset(const UnZipper* self,int fileIndex);
bool                UnZipper_fileData_read(UnZipper* self,ZipFilePos_t file_pos,
                                           unsigned char* buf,unsigned char* bufEnd);
bool                UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,const hpatch_TStreamOutput* outStream,
                                             hpatch_StreamPos_t writeToPos=0);
bool                UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,const hpatch_TStreamOutput* outStream,
                                                   hpatch_StreamPos_t writeToPos=0);
    
bool UnZipper_dataStream_copyTo(UnZipper* self,const hpatch_TStreamInput* dataStream,
                                hpatch_StreamPos_t data_begin,hpatch_StreamPos_t data_end,
                                const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos);
bool UnZipper_compressedData_decompressTo(UnZipper* self,const hpatch_TStreamInput* codeStream,
                                          hpatch_StreamPos_t code_begin,hpatch_StreamPos_t code_end,
                                          hpatch_StreamPos_t uncompressedSize,
                                          const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos);
size_t UnZipper_getHugePageAlign(const UnZipper* self,size_t baseZipAlignSize); //old4k or 16k or 64k, fail return 0 

bool UnZipper_openVirtualVCE(UnZipper* self,ZipFilePos_t fvce_size,int fileCount);
bool UnZipper_updateVirtualVCE(UnZipper* self,bool isDataNormalized,size_t zipCESize);
#if (_IS_NEED_VIRTUAL_ZIP)
void UnZipper_updateVirtualFileInfo(UnZipper* self,int fileIndex,ZipFilePos_t uncompressedSize,
                                    ZipFilePos_t compressedSize,uint32_t crc32);
#endif
static inline bool UnZipper_isHaveApkV2Sign(const UnZipper* self) { return self->_vce < self->_centralDirectory; }
static inline size_t UnZipper_ApkV2SignSize(const UnZipper* self) { return self->_centralDirectory-self->_vce; }
static inline size_t UnZipper_CESize(const UnZipper* self) {
                                        return (self->_cache_fvce+self->_fvce_size)-self->_centralDirectory; }
static inline const unsigned char* UnZipper_CEData(const UnZipper* self) { return self->_centralDirectory; }
bool UnZipper_searchApkV2Sign(const hpatch_TStreamInput* stream,hpatch_StreamPos_t centralDirectory_pos,
                              ZipFilePos_t* v2sign_topPos,ZipFilePos_t* out_blockSize=0,bool* out_isHaveV3Sign=0);
bool UnZipper_isHaveApkV1_or_jarSign(const UnZipper* self);
bool UnZipper_isHaveApkV1Sign(const UnZipper* self);
bool UnZipper_isHaveApkV3Sign(const UnZipper* self);
bool UnZipper_isHaveApkV2orV3SignTag_in_ApkV1SignFile(UnZipper* self); //found true; not found or unknown or error false
bool UnZipper_file_isApkV1_or_jarSign(const UnZipper* self,int fileIndex); // file is in ApkV1Sign Dir
bool UnZipper_file_isApkV1Sign(const UnZipper* self,int fileIndex); // file is ApkV1Sign File
bool UnZipper_file_isReCompressedByApkV2Sign(const UnZipper* self,int fileIndex);

ZipFilePos_t UnZipper_fileEntry_offset_unsafe(const UnZipper* self,int fileIndex);
int  UnZipper_searchFileIndexByName(const UnZipper* self,const char* fileName,int fileNameLen);
bool UnZipper_file_is_sameName(const UnZipper* self,int fileIndex,const char* pathName,int pathNameLen);
bool UnZipper_file_is_lastNameWith(const UnZipper* self,int fileIndex,const char* lastName,int lastNameLen);
bool UnZipper_file_is_nameEndWith(const UnZipper* self,int fileIndex,const char* nameSuffix,int nameSuffixLen);//file name is end with nameSuffix

static inline bool UnZipper_file_isStampCertFile(const UnZipper* self,int fileIndex){
    return UnZipper_file_is_lastNameWith(self,fileIndex,"stamp-cert-sha256",17);
}
    
    struct TZipCompressor{
        const void* import;
        void* (*openCompressHandle)(TZipCompressor* zipCompressor,size_t dataUncompressedSize,int compressLevel,int compressMemLevel,
                                    const hpatch_TStreamOutput* out_code,unsigned char* _mem_buf,size_t _mem_buf_size);
        bool  (*compressPart)(void* compressHandle,const unsigned char* part_data,const unsigned char* part_data_end,
                              int is_data_end,hpatch_StreamPos_t* curWritedPos);
        bool  (*closeCompressHandle)(TZipCompressor* zipCompressor,void* compressHandle);
    };
    
    struct TZipThreadWorks;
    struct TZipThreadWork;
    struct Zipper;
    struct Zipper_file_append_stream:public hpatch_TStreamOutput{
    //private:
        hpatch_StreamPos_t    inputPos;
        hpatch_StreamPos_t    outputPos;
        struct Zipper*        self;
        TZipThreadWork*       threadWork;

        TZipCompressor*       compressor;
        void*                   compressHandle;
        hpatch_TStreamOutput    compressOutStream;
        ZipFilePos_t            curFileIndex;
        static hpatch_BOOL _append_part_input(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t pos,
                                              const unsigned char* part_data,const unsigned char* part_data_end);
        static hpatch_BOOL _append_part_output(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t pos,
                                               const unsigned char* part_data,const unsigned char* part_data_end);
    };

enum TPageAlignState{
    kPageAlign_inPatch=0,
    kPageAlign_inNormalize
};

typedef struct Zipper{
//private:
    const hpatch_TStreamOutput* _stream;
    hpatch_TFileStreamOutput    _fileStream;
    ZipFilePos_t    _curFilePos;
    int             _fileEntryMaxCount;
    int             _fileEntryCount;
    size_t          _ZipAlignSize;
    size_t          _normalizeSoPageAlign;
    uint16_t*       _normalizeExtraFieldLens; //only for normalize
    TByte*          _normalizeExtraFieldBuf; //only for normalize
    bool            _pageAlignCompatible;
    int             _normalizeSoPageAlignCount;
    TPageAlignState _pageAlignState;
    int             _compressLevel;
    int             _compressMemLevel;
    int             _fileHeaderCount;
    ZipFilePos_t    _centralDirectory_pos;
    ZipFilePos_t*   _fileEntryOffsets;
    ZipFilePos_t*   _fileCompressedSizes;
    unsigned char*  _codeBuf;
    Zipper_file_append_stream _append_stream;
    //mem
    unsigned char*  _buf; //file out buf
    size_t          _curBufLen;
    
    //update crc32
    bool            _isUpdateCrc32;
    uint32_t        _newCrc32;
    //update isCompress
    bool            _isUpdateIsCompress;
    bool            _newIsCompress;
    
    //thread
    int                 _threadNum;
    TZipThreadWorks*    _threadWorks;
} Zipper;
void Zipper_init(Zipper* self);
bool Zipper_openFile(Zipper* self,const char* zipFileName,int fileEntryMaxCount,
                     int ZipAlignSize,int compressLevel,int compressMemLevel,
                     size_t normalizeSoPageAlign,bool pageAlignCompatible,TPageAlignState pageAlignState);
bool Zipper_openStream(Zipper* self,const hpatch_TStreamOutput* zipStream,int fileEntryMaxCount,
                       int ZipAlignSize,int compressLevel,int compressMemLevel,
                       size_t normalizeSoPageAlign,bool pageAlignCompatible,TPageAlignState pageAlignState);
bool Zipper_close(Zipper* self);
bool Zipper_file_append_copy(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                             bool isAlwaysReCompress=false);
    
bool Zipper_file_append_set_new_crc32(Zipper* self,uint32_t newCrc32);
bool Zipper_file_append_set_new_isCompress(Zipper* self,bool newIsCompress);
bool Zipper_file_append_begin(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                              bool appendDataIsCompressed,size_t dataUncompressedSize,size_t dataCompressedSize);
bool Zipper_file_append_beginWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                  bool appendDataIsCompressed,size_t dataUncompressedSize,size_t dataCompressedSize,
                                  int curFileCompressLevel,int curFileCompressMemLevel);
const hpatch_TStreamOutput* Zipper_file_append_part_as_stream(Zipper* self);
bool Zipper_file_append_part(Zipper* self,const unsigned char* part_data,size_t partSize);
bool Zipper_file_append_end(Zipper* self);
    
bool Zipper_copyExtra_before_fileHeader(Zipper* self,UnZipper* srcZip);
bool Zipper_fileHeader_append_set_new_crc32(Zipper* self,uint32_t newCrc32);
bool Zipper_fileHeader_append_set_new_isCompress(Zipper* self,bool newIsCompress);
bool Zipper_fileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex);
bool Zipper_endCentralDirectory_append(Zipper* self,UnZipper* srcZip);
    
size_t Zipper_compressData_maxCodeSize(size_t dataSize);
size_t Zipper_compressData(const unsigned char* data,size_t dataSize,unsigned char* out_code,
                           size_t codeSize,int kCompressLevel,int kCompressMemLevel);
void Zipper_by_multi_thread(Zipper* self,int threadNum);//need ApkV2Sign and know all file compressedSize before compress!

#ifdef __cplusplus
}
#endif
#endif //ZipPatch_Zipper_h
