//  Zipper.cpp
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
#include "Zipper.h"
#include <string.h>
#include "../../HDiffPatch/file_for_patch.h"

#include "../../zlib1.2.11/zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#define _CompressPlugin_zlib
#define _IsNeedIncludeDefaultCompressHead 0
#include "../../HDiffPatch/compress_plugin_demo.h"
#include "../../HDiffPatch/decompress_plugin_demo.h"
static const hdiff_TStreamCompress* compressPlugin  =&zlibStreamCompressPlugin;
static hpatch_TDecompress*    decompressPlugin=&zlibDecompressPlugin;

#define     kZipAlignSize  8
#define     kZlibVesion    "1.2.11" //fixed zlib version
#define     kCompressLevel 7        //fixed Compress Level, for patch speed

#define check(v) { if (!(v)) { assert(false); return false; } }

inline static uint16_t readUInt16(const TByte* buf){
    return buf[0]|(buf[1]<<8);
}
inline static uint32_t readUInt32(const TByte* buf){
    return buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);
}

#define kIsAlignFileInfo            false
#define kBufSize                    (16*1024)

//https://en.wikipedia.org/wiki/Zip_(file_format)
#define kMaxEndGlobalComment        (1<<(2*8)) //2 byte
#define kMinEndCentralDirectorySize (22)  //struct size
#define kENDHEADERMAGIC             (0x06054b50)
#define kLOCALHEADERMAGIC           (0x04034b50)
#define kCENTRALHEADERMAGIC         (0x02014b50)
#define kMinFileHeaderSize          (46)  //struct size

//反向找到第一个kENDHEADERMAGIC的位置;
static bool _UnZipper_searchEndCentralDirectory(UnZipper* self,ZipFilePos_t* out_endCDir_pos){
    TByte*       buf=self->_buf;
    ZipFilePos_t max_back_pos = kMaxEndGlobalComment+kMinEndCentralDirectorySize;
    if (max_back_pos>self->_fileLength)
        max_back_pos=self->_fileLength;
    ZipFilePos_t readed_pos=0;
    uint32_t     cur_value=0;
    while (readed_pos<max_back_pos) {
        ZipFilePos_t readLen=max_back_pos-readed_pos;
        if (readLen>kBufSize) readLen=kBufSize;
        readed_pos+=readLen;
        check(UnZipper_fileData_read(self,self->_fileLength-readed_pos,buf,buf+readLen));
        for (int i=(int)readLen-1; i>=0; --i) {
            cur_value=(cur_value<<8)|buf[i];
            if (cur_value==kENDHEADERMAGIC){
                *out_endCDir_pos=(self->_fileLength-readed_pos)+i;
                return true;//ok
            }
        }
    }
    return false;//not found
}


int UnZipper_fileCount(const UnZipper* self){
    return readUInt16(self->_endCentralDirectoryInfo+8);
}
static inline int32_t _centralDirectory_size(const UnZipper* self){
    return readUInt32(self->_endCentralDirectoryInfo+12);
}
static inline int32_t _centralDirectory_pos(const UnZipper* self){
    return readUInt32(self->_endCentralDirectoryInfo+16);
}

//读取EndCentralDirectory的有用信息;
static bool _UnZipper_ReadEndCentralDirectory(UnZipper* self){
    TByte* buf=self->_endCentralDirectoryInfo;
    return UnZipper_fileData_read(self,self->_endCentralDirectory_pos,buf,buf+kMinEndCentralDirectorySize);
}


inline static const TByte* fileHeaderBuf(const UnZipper* self,int fileIndex){
    return self->_cache_fileHeaders+self->_fileHeaderOffsets[fileIndex];
}

int UnZipper_file_nameLen(const UnZipper* self,int fileIndex){
    const TByte* headBuf=fileHeaderBuf(self,fileIndex);
    return readUInt16(headBuf+28);
}

const unsigned char* UnZipper_file_nameBegin(const UnZipper* self,int fileIndex){
    const TByte* headBuf=fileHeaderBuf(self,fileIndex);
    return headBuf+kMinFileHeaderSize;
}

//缓存所有FileHeader数据和其在缓存中的位置;
static bool _UnZipper_cacheFileHeader(UnZipper* self){
    assert(self->_cache_fileHeaders==0);
    const int fileCount=UnZipper_fileCount(self);
    const uint32_t centralDirectory_size=_centralDirectory_size(self);
    TByte* buf=(TByte*)malloc(centralDirectory_size+sizeof(ZipFilePos_t)*(fileCount+1));
    self->_cache_fileHeaders=buf;
    const uint32_t centralDirectory_pos=_centralDirectory_pos(self);
    check(UnZipper_fileData_read(self,centralDirectory_pos,buf,buf+centralDirectory_size));
    
    size_t alignBuf=_hpatch_align_upper((buf+centralDirectory_size),sizeof(ZipFilePos_t));
    self->_fileHeaderOffsets=(ZipFilePos_t*)alignBuf;
    int curOffset=0;
    for (int i=0; i<fileCount; ++i) {
        check(kCENTRALHEADERMAGIC==readUInt32(buf+curOffset));
        self->_fileHeaderOffsets[i]=curOffset;
        int fileNameLen=UnZipper_file_nameLen(self,i);
        int extraFieldLen=readUInt16(buf+curOffset+30);
        int fileCommentLen=readUInt16(buf+curOffset+32);
        curOffset+= kMinFileHeaderSize + fileNameLen+extraFieldLen+fileCommentLen;
        check(curOffset <= centralDirectory_size);
    }
    return true;
}

void UnZipper_init(UnZipper* self){
    memset(self,0,sizeof(*self));
}
bool UnZipper_close(UnZipper* self){
    self->_file_curPos=0;
    self->_fileLength=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    if (self->_cache_fileHeaders) { free(self->_cache_fileHeaders); self->_cache_fileHeaders=0; }
    return fileClose(&self->_file);
}

static long _stream_read(void* _self,hpatch_StreamPos_t file_pos,unsigned char* buf,unsigned char* bufEnd){
    if (file_pos!=(ZipFilePos_t)file_pos) return 0;
    UnZipper* self=(UnZipper*)_self;
    if (!UnZipper_fileData_read(self,(ZipFilePos_t)file_pos,buf,bufEnd))
        return 0;
    else
        return (long)(bufEnd-buf);
}

bool UnZipper_openRead(UnZipper* self,const char* zipFileName){
    hpatch_StreamPos_t fileLength=0;
    assert(self->_file==0);
    check(fileOpenForRead(zipFileName,&self->_file,&fileLength));
    self->_file_curPos=0;
    self->_fileLength=(ZipFilePos_t)fileLength;
    check(self->_fileLength==fileLength);
    assert(self->_buf==0);
    self->_buf=(unsigned char*)malloc(kBufSize+kMinEndCentralDirectorySize);
    self->_endCentralDirectoryInfo=self->_buf+kBufSize;
    check(_UnZipper_searchEndCentralDirectory(self,&self->_endCentralDirectory_pos));
    check(_UnZipper_ReadEndCentralDirectory(self));
    check(_UnZipper_cacheFileHeader(self));
    self->_stream.streamHandle=self;
    self->_stream.streamSize=fileLength;
    self->_stream.read=_stream_read;
    return true;
}

static uint16_t  _file_compressType(const UnZipper* self,int fileIndex){
    return readUInt16(fileHeaderBuf(self,fileIndex)+10);
}
bool  UnZipper_file_isCompressed(const UnZipper* self,int fileIndex){
    uint16_t compressType=_file_compressType(self,fileIndex);
    return compressType!=0;
}
ZipFilePos_t UnZipper_file_compressedSize(const UnZipper* self,int fileIndex){
    return readUInt32(fileHeaderBuf(self,fileIndex)+20);
}
ZipFilePos_t UnZipper_file_uncompressedSize(const UnZipper* self,int fileIndex){
    return readUInt32(fileHeaderBuf(self,fileIndex)+24);
}

ZipFilePos_t UnZipper_fileData_offset(UnZipper* self,int fileIndex){
    const ZipFilePos_t entryOffset=readUInt32(fileHeaderBuf(self,fileIndex)+42);
    TByte buf[4];
    check(UnZipper_fileData_read(self,entryOffset+26,buf,buf+4));
    return entryOffset+30+readUInt16(buf)+readUInt16(buf+2);
}

bool UnZipper_fileData_read(UnZipper* self,ZipFilePos_t file_pos,unsigned char* buf,unsigned char* bufEnd){
    //当前的实现不支持多线程;
    ZipFilePos_t curPos=self->_file_curPos;
    if (file_pos!=curPos)
        check(fileSeek64(self->_file,file_pos,SEEK_SET));
    self->_file_curPos=file_pos+(ZipFilePos_t)(bufEnd-buf);
    assert(self->_file_curPos<=self->_fileLength);
    return fileRead(self->_file,buf,bufEnd);
}

bool UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,const hpatch_TStreamOutput* outStream){
    TByte* buf=self->_buf;
    ZipFilePos_t fileSavedSize=UnZipper_file_compressedSize(self,fileIndex);
    ZipFilePos_t fileDataOffset=UnZipper_fileData_offset(self,fileIndex);
    ZipFilePos_t curWritePos=0;
    while (curWritePos<fileSavedSize) {
        long readLen=kBufSize;
        if ((ZipFilePos_t)readLen>(fileSavedSize-curWritePos)) readLen=(long)(fileSavedSize-curWritePos);
        check(UnZipper_fileData_read(self,fileDataOffset+curWritePos,buf,buf+readLen));
        check(readLen==outStream->write(outStream->streamHandle,curWritePos,buf,buf+readLen));
        curWritePos+=readLen;
    }
    return true;
}

#define check_clear(v) { if (!(v)) { result=false; goto clear; } }

bool UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,const hpatch_TStreamOutput* outStream){
    if (!UnZipper_file_isCompressed(self,fileIndex)){
        return UnZipper_fileData_copyTo(self,fileIndex,outStream);
    }
    
    uint16_t compressType=_file_compressType(self,fileIndex);
    check(Z_DEFLATED==compressType);
    
    bool result=true;
    ZipFilePos_t file_offset=UnZipper_fileData_offset(self,fileIndex);
    ZipFilePos_t file_compressedSize=UnZipper_file_compressedSize(self,fileIndex);
    ZipFilePos_t file_data_size=UnZipper_file_uncompressedSize(self,fileIndex);
    
    _zlib_TDecompress* decHandle=_zlib_decompress_open_by(decompressPlugin,&self->_stream,file_offset,
                                                          file_offset+file_compressedSize,0,
                                                          self->_buf,(kBufSize>>1));
    check(decHandle!=0);
    TByte* dataBuf=self->_buf+(kBufSize>>1);
    ZipFilePos_t  curWritePos=0;
    while (curWritePos<file_data_size){
        long readLen=(kBufSize>>1);
        if ((ZipFilePos_t)readLen>(file_data_size-curWritePos)) readLen=(long)(file_data_size-curWritePos);
        check(readLen==_zlib_decompress_part(decompressPlugin,decHandle,dataBuf,dataBuf+readLen));
        check(readLen==outStream->write(outStream->streamHandle,curWritePos,dataBuf,dataBuf+readLen));
        curWritePos+=readLen;
    }
    check(_zlib_is_decompress_finish(decompressPlugin,decHandle));
clear:
    _zlib_decompress_close_by(decompressPlugin,decHandle);
    return result;
}

//----

void Zipper_init(Zipper* self){
    memset(self,0,sizeof(*self));
}
bool Zipper_close(Zipper* self){
    self->_fileEntryCount=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    return fileClose(&self->_file);
}

bool Zipper_openWrite(Zipper* self,const char* zipFileName,int fileEntryMaxCount){
    assert(self->_file==0);
    check(fileOpenForCreateOrReWrite(zipFileName,&self->_file));
    TByte* buf=(TByte*)malloc(kBufSize*2+sizeof(ZipFilePos_t)*(fileEntryMaxCount+1)
                              +fileEntryMaxCount*sizeof(uint32_t)+fileEntryMaxCount*sizeof(TByte));
    self->_buf=buf;
    self->_codeBuf=buf+kBufSize;
    size_t alignBuf=_hpatch_align_upper((buf+kBufSize*2),sizeof(ZipFilePos_t));
    self->_fileEntryOffsets=(ZipFilePos_t*)alignBuf;
    self->_fileCompressedSizes=(uint32_t*)(self->_fileEntryOffsets+fileEntryMaxCount);
    memset(self->_fileCompressedSizes,0,fileEntryMaxCount*sizeof(uint32_t));//set error len
    self->_extFieldLens=(TByte*)(self->_fileCompressedSizes+fileEntryMaxCount);
    memset(self->_extFieldLens,0xff,fileEntryMaxCount*sizeof(TByte));//set error len
    self->_fileEntryCount=0;
    self->_fileEntryMaxCount=fileEntryMaxCount;
    self->_fileHeaderCount=0;
    self->_curFilePos=0;
    self->_centralDirectory_pos=0;
    return true;
}

static bool _writeFlush(Zipper* self){
    size_t curBufLen=self->_curBufLen;
    if (curBufLen>0){
        self->_curBufLen=0;
        return fileWrite(self->_file,self->_buf,self->_buf+curBufLen);
    }else{
        return true;
    }
}
static bool _write(Zipper* self,const TByte* data,size_t len){
    self->_curFilePos+=len;
    size_t curBufLen=self->_curBufLen;
    if (((curBufLen>0)||(len*2<=kBufSize)) && (curBufLen+len<=kBufSize)){//to buf
        memcpy(self->_buf+curBufLen,data,len);
        self->_curBufLen=curBufLen+len;
        return true;
    }else{
        if (curBufLen>0)
            check(_writeFlush(self));
        return fileWrite(self->_file,data,data+len);
    }
}


inline static void _writeUInt32_to(TByte* out_buf4,uint32_t v){
    out_buf4[0]=(TByte)v; out_buf4[1]=(TByte)(v>>8);
    out_buf4[2]=(TByte)(v>>16); out_buf4[3]=(TByte)(v>>24);
}

inline static bool _writeUInt32(Zipper* self,uint32_t v){
    TByte buf[4];
    _writeUInt32_to(buf,v);
    return _write(self,buf,4);
}
inline static bool _writeUInt16(Zipper* self,uint16_t v){
    TByte buf[2];
    buf[0]=(TByte)v; buf[1]=(TByte)(v>>8);
    return _write(self,buf,2);
}


const TByte _alignSkipBuf[kZipAlignSize]={0};
inline static size_t _getAlignSkipLen(size_t curPos){
    return _hpatch_align_upper(curPos,kZipAlignSize)-curPos;
}
inline static bool _writeAlignSkip(Zipper* self,size_t alignSkipLen){
    assert(alignSkipLen<=kZipAlignSize);
    return _write(self,_alignSkipBuf,alignSkipLen);
}

inline static bool _writeToAlign(Zipper* self){
    size_t alignSkipLen=_getAlignSkipLen(self->_curFilePos);
    if (alignSkipLen>0)
        return _writeAlignSkip(self,alignSkipLen);
    else
        return true;
}

#define assert_align(self) assert((self->_curFilePos&(kZipAlignSize-1))==0);

bool _write_fileHeaderInfo(Zipper* self,int fileIndex,UnZipper* srcZip,int srcFileIndex,bool isFullInfo){
    if (kIsAlignFileInfo) assert_align(self);
    const TByte* headBuf=fileHeaderBuf(srcZip,srcFileIndex);
    check(_writeUInt32(self,isFullInfo?kCENTRALHEADERMAGIC:kLOCALHEADERMAGIC));
    if (isFullInfo)
        check(_write(self,headBuf+4,2));//压缩版本;
    check(_write(self,headBuf+6,2));//解压缩版本;
    uint16_t fileTag=readUInt16(headBuf+8);//标志;
    check(_writeUInt16(self,fileTag&(~(1<<3))));//标志中去掉Data descriptor标识;
    check(_write(self,headBuf+10,20-10));//压缩方式--CRC-32校验;
    check(_writeUInt32(self,self->_fileCompressedSizes[fileIndex]));//压缩后大小;
    check(_write(self,headBuf+24,30-24));//压缩前大小--文件名称长度;
    
    bool     isCompressed=UnZipper_file_isCompressed(srcZip,srcFileIndex);
    uint16_t fileNameLen=UnZipper_file_nameLen(srcZip,srcFileIndex);
    
    uint16_t extFieldLen=0;
    if (!isFullInfo){
        extFieldLen=isCompressed ? 0 : //压缩文件不需要对齐;
            _getAlignSkipLen(self->_curFilePos+2+fileNameLen);//利用 扩展字段 对齐;
        self->_extFieldLens[fileIndex]=(TByte)extFieldLen;
    }else{
        extFieldLen=self->_extFieldLens[fileIndex];//已经设置过了 扩展字段 的长度;
    }
    check(_writeUInt16(self,extFieldLen));//扩展字段长度;
    
    uint16_t fileCommentLen=0;
    if (isFullInfo){
        fileCommentLen = (!kIsAlignFileInfo) ? 0:
            _getAlignSkipLen(self->_curFilePos+2+(42-34)+4+fileNameLen+extFieldLen);//利用 文件注释 对齐;
        check(_writeUInt16(self,fileCommentLen));//文件注释长度;
        check(_write(self,headBuf+34,42-34));//文件开始的分卷号--文件外部属性;
        check(_writeUInt32(self,self->_fileEntryOffsets[fileIndex]));//对应文件实体在文件中的偏移;
    }
    
    check(_write(self,UnZipper_file_nameBegin(srcZip,srcFileIndex),fileNameLen));//文件名;
    if (extFieldLen>0)
        check(_writeAlignSkip(self,extFieldLen));
    if (fileCommentLen>0)
        check(_writeAlignSkip(self,fileCommentLen));
    if (kIsAlignFileInfo&&((!isCompressed)||(isFullInfo))) assert_align(self);
    return  true;
}


long Zipper_file_append_stream::_append_part_input(hpatch_TStreamOutputHandle handle,hpatch_StreamPos_t pos,
                                                   const unsigned char *part_data,
                                                   const unsigned char *part_data_end){
    Zipper_file_append_stream* append_state=(Zipper_file_append_stream*)handle;
    assert(append_state->inputPos==pos);
    long partSize=(long)(part_data_end-part_data);
    append_state->inputPos+=partSize;
    if (append_state->compressHandle){
        int outStream_isCanceled=0;//false
        hpatch_StreamPos_t curWritedPos=append_state->outputPos;
        int is_data_end=(append_state->inputPos==append_state->streamSize);
        if(!_zlib_compress_stream_part(compressPlugin,append_state->compressHandle,part_data,part_data_end,
                                       is_data_end,&curWritedPos,&outStream_isCanceled)) return 0;//error
        return partSize;
    }else{
        return _append_part_output(handle,append_state->outputPos,part_data,part_data_end);
    }
}

long Zipper_file_append_stream::_append_part_output(hpatch_TStreamOutputHandle handle,hpatch_StreamPos_t pos,
                                                    const unsigned char *part_data,
                                                    const unsigned char *part_data_end){
    Zipper_file_append_stream* append_state=(Zipper_file_append_stream*)handle;
    assert(append_state->outputPos==pos);
    long partSize=(long)(part_data_end-part_data);
    append_state->outputPos+=partSize;
    if (_write(append_state->self,part_data,partSize))
        return partSize;
    else
        return 0; //error
}

bool Zipper_file_append_begin(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                              size_t dataSize,bool dataIsCompressed){
    assert(dataSize==(uint32_t)dataSize);
    Zipper_file_append_stream* append_state=&self->_append_stream;
    if (append_state->self!=0){
        assert(false);  //need call Zipper_file_append_end()
        return false;
    }
    const bool isCompressed=UnZipper_file_isCompressed(srcZip,srcFileIndex);
    if ((!isCompressed)&&(dataIsCompressed)){
        assert(false);  //now need input decompressed data;
        return 0;       // for example: UnZipper_fileData_decompressTo(Zipper_file_append_part_as_stream());
    }
    
    ZipFilePos_t curFileIndex=self->_fileEntryCount;
    check(curFileIndex < self->_fileEntryMaxCount);
    self->_fileEntryOffsets[curFileIndex]=self->_curFilePos;
    self->_fileCompressedSizes[curFileIndex]=(!isCompressed)?(uint32_t)dataSize: //finally value
                                              UnZipper_file_compressedSize(srcZip,srcFileIndex);//temp value
    self->_fileEntryCount=curFileIndex+1;
    check(_write_fileHeaderInfo(self,curFileIndex,srcZip,srcFileIndex,false));//out file info
    
    append_state->self=self;
    append_state->inputPos=0;
    append_state->outputPos=0;
    append_state->curFileIndex=curFileIndex;
    append_state->streamHandle=append_state;
    append_state->streamSize=dataSize;
    append_state->write=Zipper_file_append_stream::_append_part_input;
    if (isCompressed&&(!dataIsCompressed)){//compress data
        append_state->compressOutStream.streamHandle=append_state;
        append_state->compressOutStream.streamSize=self->_fileCompressedSizes[curFileIndex];
        append_state->compressOutStream.write=Zipper_file_append_stream::_append_part_output;
        append_state->compressHandle=_zlib_compress_open_by(compressPlugin,&append_state->compressOutStream,
                                                            0,kCompressLevel,self->_codeBuf,kBufSize);
    }else{
        append_state->compressHandle=0; //copy data
    }
    return true;
}

bool _zipper_file_update_compressedSize(Zipper* self,ZipFilePos_t curFileIndex,uint32_t compressedSize){
    if (curFileIndex>=self->_fileEntryCount){ assert(false); return false; }
    if (self->_fileCompressedSizes[curFileIndex]==compressedSize) return true;
    self->_fileCompressedSizes[curFileIndex]=compressedSize;
    
    uint32_t fileEntryOffset=self->_fileEntryOffsets[curFileIndex];
    uint32_t compressedSizeOffset=fileEntryOffset+18;
    TByte buf[4];
    _writeUInt32_to(buf,compressedSize);
    
    //todo: optimize, is in cache buf?
    check(_writeFlush(self));
    ZipFilePos_t backFilePos=self->_curFilePos;
    check(fileSeek64(self->_file,compressedSizeOffset,SEEK_SET));
    check(fileWrite(self->_file,buf,buf+4));
    check(fileSeek64(self->_file,backFilePos,SEEK_SET));
    return true;
}

bool Zipper_file_append_end(Zipper* self){
    Zipper_file_append_stream* append_state=&self->_append_stream;
    if (append_state->self==0) { assert(false); return false; }
    
    bool result=true;
    if (append_state->compressHandle!=0){
        check_clear(_zlib_compress_close_by(compressPlugin,append_state->compressHandle));
    }
    
    check_clear(append_state->inputPos==append_state->streamSize);
    if (kIsAlignFileInfo) check_clear(_writeToAlign(self));//填充数据对齐;
    
    if (append_state->compressHandle!=0){
        assert(append_state->outputPos==(uint32_t)append_state->outputPos);
        uint32_t compressedSize=(uint32_t)append_state->outputPos;
        check_clear(_zipper_file_update_compressedSize(self,append_state->curFileIndex,compressedSize));
    }
clear:
    memset(append_state,0,sizeof(Zipper_file_append_stream));
    return result;
}

const hpatch_TStreamOutput* Zipper_file_append_part_as_stream(Zipper* self){
    Zipper_file_append_stream* append_state=&self->_append_stream;
    if (append_state->self==0) { assert(false); return 0; }
    return append_state;
}

bool Zipper_file_append_part(Zipper* self,const unsigned char* part_data,size_t partSize){
    Zipper_file_append_stream* append_state=&self->_append_stream;
    if (append_state->self==0) { assert(false); return false; }
    return (partSize==append_state->write(append_state->streamHandle,append_state->inputPos,
                                          part_data,part_data+partSize));
}

bool Zipper_file_append_copy(Zipper* self,UnZipper* srcZip,int srcFileIndex){
    size_t dataSize=UnZipper_file_compressedSize(srcZip,srcFileIndex);
    bool   dataIsCompressed=UnZipper_file_isCompressed(srcZip,srcFileIndex);
    
    check(Zipper_file_append_begin(self,srcZip,srcFileIndex,dataSize,dataIsCompressed));
    check(UnZipper_fileData_copyTo(srcZip,srcFileIndex,&self->_append_stream));
    assert(self->_append_stream.inputPos==dataSize);
    check(Zipper_file_append_end(self));
    return true;
}
bool Zipper_file_append_data(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                             const unsigned char* data,size_t dataSize,bool dataIsCompressed){
    check(Zipper_file_append_begin(self,srcZip,srcFileIndex,dataSize,dataIsCompressed));
    check(Zipper_file_append_part(self,data,dataSize));
    check(Zipper_file_append_end(self));
    return true;
}

bool Zipper_fileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex){
    ZipFilePos_t curFileIndex=self->_fileHeaderCount;
    check(curFileIndex < self->_fileEntryCount);
    self->_fileHeaderCount=curFileIndex+1;
    if (curFileIndex==0)
        self->_centralDirectory_pos=self->_curFilePos;
    return _write_fileHeaderInfo(self,curFileIndex,srcZip,srcFileIndex,true);
}

bool Zipper_endCentralDirectory_append(Zipper* self,UnZipper* srcZip){
    if (kIsAlignFileInfo) assert_align(self);
    check(self->_fileEntryCount==self->_fileHeaderCount);
    const TByte* endBuf=srcZip->_endCentralDirectoryInfo;
    uint32_t centralDirectory_size=self->_curFilePos-self->_centralDirectory_pos;
    
    check(_write(self,endBuf+0,10-0));//固定魔法值--当前分卷Central Directory的记录数量;
    check(_writeUInt16(self,self->_fileEntryCount));
    check(_writeUInt32(self,centralDirectory_size));
    check(_writeUInt32(self,self->_centralDirectory_pos));
    check(_writeUInt16(self,0));//Zip文件注释长度;
    return  _writeFlush(self);  
}

