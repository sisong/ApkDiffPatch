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

inline static void writeUInt16_to(TByte* out_buf2,uint32_t v){
    out_buf2[0]=(TByte)v; out_buf2[1]=(TByte)(v>>8);
}
inline static void writeUInt32_to(TByte* out_buf4,uint32_t v){
    out_buf4[0]=(TByte)v; out_buf4[1]=(TByte)(v>>8);
    out_buf4[2]=(TByte)(v>>16); out_buf4[3]=(TByte)(v>>24);
}

#define kBufSize                    (64*1024)

//https://en.wikipedia.org/wiki/Zip_(file_format)
#define kMaxEndGlobalComment        (1<<(2*8)) //2 byte
#define kMinEndCentralDirectorySize (22)  //struct size
#define kENDHEADERMAGIC             (0x06054b50)
#define kLOCALHEADERMAGIC           (0x04034b50)
#define kCENTRALHEADERMAGIC         (0x02014b50)
#define kMinFileHeaderSize          (46)  //struct size



//反向找到第一个kENDHEADERMAGIC的位置;
static bool _UnZipper_searchEndCentralDirectory(UnZipper* self,ZipFilePos_t* out_endCentralDirectory_pos){
    TByte*       buf=self->_buf;
    ZipFilePos_t max_back_pos = kMaxEndGlobalComment+kMinEndCentralDirectorySize;
    ZipFilePos_t fileLength=(ZipFilePos_t)self->stream->streamSize;
    if (max_back_pos>fileLength)
        max_back_pos=fileLength;
    ZipFilePos_t readed_pos=0;
    uint32_t     cur_value=0;
    while (readed_pos<max_back_pos) {
        ZipFilePos_t readLen=max_back_pos-readed_pos;
        if (readLen>kBufSize) readLen=kBufSize;
        readed_pos+=readLen;
        check(readLen==self->stream->read(self->stream->streamHandle,fileLength-readed_pos,buf,buf+readLen));
        for (int i=(int)readLen-1; i>=0; --i) {
            cur_value=(cur_value<<8)|buf[i];
            if (cur_value==kENDHEADERMAGIC){
                *out_endCentralDirectory_pos=(fileLength-readed_pos)+i;
                return true;//ok
            }
        }
    }
    return false;//not found
}

static bool _UnZipper_searchCentralDirectory(UnZipper* self,ZipFilePos_t endCentralDirectory_pos,
                                             ZipFilePos_t* out_centralDirectory_pos,uint32_t* out_fileCount){
    const int readLen=20-8;
    TByte buf[readLen];
    check(readLen==self->stream->read(self->stream->streamHandle,endCentralDirectory_pos+8,buf,buf+readLen));
    *out_fileCount=readUInt16(buf+(8-8));
    *out_centralDirectory_pos=readUInt32(buf+(16-8));
    return true;
}

static bool _UnZipper_searchApkV2Sign(UnZipper* self,ZipFilePos_t centralDirectory_pos,
                                             ZipFilePos_t* v2sign_pos,bool* isApkNormalized){
    //todo: searchApkV2Sign
    *v2sign_pos=centralDirectory_pos;
    return true;
}

static ZipFilePos_t _fileData_offset_read(UnZipper* self,ZipFilePos_t entryOffset){
    TByte buf[4];
    check(UnZipper_fileData_read(self,entryOffset+26,buf,buf+4));
    return entryOffset+30+readUInt16(buf)+readUInt16(buf+2);
}

int UnZipper_fileCount(const UnZipper* self){
    return readUInt16(self->_endCentralDirectory+8);
}
static inline int32_t _centralDirectory_size(const UnZipper* self){
    return readUInt32(self->_endCentralDirectory+12);
}

inline static const TByte* fileHeaderBuf(const UnZipper* self,int fileIndex){
    return self->_centralDirectory+self->_fileHeaderOffsets[fileIndex];
}

int UnZipper_file_nameLen(const UnZipper* self,int fileIndex){
    const TByte* headBuf=fileHeaderBuf(self,fileIndex);
    return readUInt16(headBuf+28);
}

const char* UnZipper_file_nameBegin(const UnZipper* self,int fileIndex){
    const TByte* headBuf=fileHeaderBuf(self,fileIndex);
    return (char*)headBuf+kMinFileHeaderSize;
}

//缓存相关信息并规范化数据;
static bool _UnZipper_vce_normalized(UnZipper* self,bool isHeaderMatch){
    assert(self->_endCentralDirectory!=0);
    writeUInt32_to(self->_endCentralDirectory+16,
                   (uint32_t)(self->_centralDirectory-self->_cache_vce));//normalized CentralDirectory的开始位置偏移;
    
    assert(self->_centralDirectory!=0);
    TByte* buf=self->_centralDirectory;
    const int fileCount=UnZipper_fileCount(self);
    size_t centralDirectory_size=_centralDirectory_size(self);
    int curOffset=0;
    for (int i=0; i<fileCount; ++i) {
        TByte* headBuf=buf+curOffset;
        check(kCENTRALHEADERMAGIC==readUInt32(headBuf));
        self->_fileHeaderOffsets[i]=curOffset;
        
        self->_fileCompressedSizes[i]=readUInt32(headBuf+20);
        writeUInt32_to(headBuf+20,0);//normalized 压缩大小占位;
        
        uint32_t fileEntryOffset=readUInt32(headBuf+42);
        writeUInt32_to(headBuf+42,0);//normalized 文件Entry开始位置偏移;
        self->_fileDataOffsets[i]=isHeaderMatch? (fileEntryOffset+readUInt16(headBuf+28)+readUInt16(headBuf+30)): //unsafe
                                            _fileData_offset_read(self,fileEntryOffset);//较慢;
        
        uint16_t fileTag=readUInt16(headBuf+8);//标志;
        writeUInt16_to(headBuf+8,fileTag&(~(1<<3)));//normalized 标志中去掉Data descriptor标识;
        
        int fileNameLen=UnZipper_file_nameLen(self,i);
        int extraFieldLen=readUInt16(headBuf+30);
        int fileCommentLen=readUInt16(headBuf+32);
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
    if (self->_cache_vce) { free(self->_cache_vce); self->_cache_vce=0; }
    return fileClose(&self->_file);
}

static long _stream_read_file(void* _self,hpatch_StreamPos_t file_pos,unsigned char* buf,unsigned char* bufEnd){
    if (file_pos!=(ZipFilePos_t)file_pos) return 0;
    UnZipper* self=(UnZipper*)_self;
    if (!UnZipper_fileData_read(self,(ZipFilePos_t)file_pos,buf,bufEnd))
        return 0;
    else
        return (long)(bufEnd-buf);
}


static bool _UnZipper_openRead_begin(UnZipper* self){
    assert(self->_buf==0);
    self->_buf=(unsigned char*)malloc(kBufSize);
    check(self->_buf!=0);
    self->_isApkNormalized=false;
    return true;
}

static bool _UnZipper_openRead_vce(UnZipper* self,ZipFilePos_t vceSize,int fileCount){
    assert(self->_cache_vce==0);
    self->_vce_size=vceSize;
    self->_cache_vce=(TByte*)malloc(self->_vce_size+sizeof(ZipFilePos_t)*(fileCount+1)+sizeof(uint32_t)*fileCount*2);
    size_t alignBuf=_hpatch_align_upper((self->_cache_vce+self->_vce_size),sizeof(ZipFilePos_t));
    self->_fileDataOffsets=(ZipFilePos_t*)alignBuf;
    self->_fileHeaderOffsets=(uint32_t*)(self->_fileDataOffsets+fileCount);
    self->_fileCompressedSizes=(uint32_t*)(self->_fileHeaderOffsets+fileCount);
    return true;
}


static bool _UnZipper_openRead_file(UnZipper* self,const char* zipFileName){
    hpatch_StreamPos_t fileLength=0;
    assert(self->_file==0);
    check(fileOpenForRead(zipFileName,&self->_file,&fileLength));
    self->_file_curPos=0;
    self->_fileLength=(ZipFilePos_t)fileLength;
    check(self->_fileLength==fileLength);
    self->_stream.streamHandle=self;
    self->_stream.streamSize=fileLength;
    self->_stream.read=_stream_read_file;
    self->stream=&self->_stream;
    return true;
}

#define _UnZipper_sreachVCE() \
    ZipFilePos_t endCentralDirectory_pos=0; \
    ZipFilePos_t centralDirectory_pos=0;    \
    uint32_t     fileCount=0;   \
    ZipFilePos_t v2sign_pos=0;  \
    check(_UnZipper_searchEndCentralDirectory(self,&endCentralDirectory_pos));  \
    check(_UnZipper_searchCentralDirectory(self,endCentralDirectory_pos,&centralDirectory_pos,&fileCount)); \
    check(_UnZipper_searchApkV2Sign(self,centralDirectory_pos,&v2sign_pos,&self->_isApkNormalized));


bool UnZipper_openRead(UnZipper* self,const char* zipFileName){
    check(_UnZipper_openRead_begin(self));
    check(_UnZipper_openRead_file(self,zipFileName));
    _UnZipper_sreachVCE();
    check(_UnZipper_openRead_vce(self,self->_fileLength-v2sign_pos,fileCount));
    self->_centralDirectory=self->_cache_vce+(centralDirectory_pos-v2sign_pos);
    self->_endCentralDirectory=self->_cache_vce+(endCentralDirectory_pos-v2sign_pos);
    
    check(UnZipper_fileData_read(self,v2sign_pos,self->_cache_vce,self->_cache_vce+self->_vce_size));
    
    bool isHeaderMatch=self->_isApkNormalized;
    check(_UnZipper_vce_normalized(self,isHeaderMatch));
    
    return true;
}

static long _stream_read_vce(void* _self,hpatch_StreamPos_t file_pos,unsigned char* buf,unsigned char* bufEnd){
    UnZipper* self=(UnZipper*)_self;
    long result=(long)(bufEnd-buf);
    if (file_pos+result<=self->_vce_size){
        memcpy(buf,self->_cache_vce+file_pos,result);
        return result;
    }else{
        return 0;
    }
}
bool UnZipper_openForVCE(UnZipper* self,ZipFilePos_t vce_size,int fileCount){
    check(_UnZipper_openRead_begin(self));
    
    self->_stream.streamHandle=self;
    self->_stream.streamSize=vce_size;
    self->_stream.read=_stream_read_vce;
    self->stream=&self->_stream;
    
    check(_UnZipper_openRead_vce(self,vce_size,fileCount));
    return true;
}

bool UnZipper_updateVCE(UnZipper* self){
    _UnZipper_sreachVCE();
    self->_centralDirectory=self->_cache_vce+(centralDirectory_pos-v2sign_pos);
    self->_endCentralDirectory=self->_cache_vce+(endCentralDirectory_pos-v2sign_pos);
    
    bool isHeaderMatch=true;
    check(_UnZipper_vce_normalized(self,isHeaderMatch));
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
    return self->_fileCompressedSizes[fileIndex];
}
ZipFilePos_t UnZipper_file_uncompressedSize(const UnZipper* self,int fileIndex){
    return readUInt32(fileHeaderBuf(self,fileIndex)+24);
}

uint32_t UnZipper_file_crc32(const UnZipper* self,int fileIndex){
    return readUInt32(fileHeaderBuf(self,fileIndex)+16);
}

ZipFilePos_t UnZipper_fileData_offset(UnZipper* self,int fileIndex){
    return self->_fileDataOffsets[fileIndex];
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

bool UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,
                              const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos){
    TByte* buf=self->_buf;
    ZipFilePos_t fileSavedSize=UnZipper_file_compressedSize(self,fileIndex);
    ZipFilePos_t fileDataOffset=UnZipper_fileData_offset(self,fileIndex);
    ZipFilePos_t curWritePos=0;
    while (curWritePos<fileSavedSize) {
        long readLen=kBufSize;
        if ((ZipFilePos_t)readLen>(fileSavedSize-curWritePos)) readLen=(long)(fileSavedSize-curWritePos);
        check(UnZipper_fileData_read(self,fileDataOffset+curWritePos,buf,buf+readLen));
        check(readLen==outStream->write(outStream->streamHandle,writeToPos+curWritePos,buf,buf+readLen));
        curWritePos+=readLen;
    }
    return true;
}

#define check_clear(v) { if (!(v)) { result=false; goto clear; } }

bool UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,
                                    const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos){
    if (!UnZipper_file_isCompressed(self,fileIndex)){
        return UnZipper_fileData_copyTo(self,fileIndex,outStream,writeToPos);
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
        check(readLen==outStream->write(outStream->streamHandle,writeToPos+curWritePos,dataBuf,dataBuf+readLen));
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
                              +fileEntryMaxCount*sizeof(uint32_t));
    self->_buf=buf;
    self->_codeBuf=buf+kBufSize;
    size_t alignBuf=_hpatch_align_upper((buf+kBufSize*2),sizeof(ZipFilePos_t));
    self->_fileEntryOffsets=(ZipFilePos_t*)alignBuf;
    self->_fileCompressedSizes=(uint32_t*)(self->_fileEntryOffsets+fileEntryMaxCount);
    memset(self->_fileCompressedSizes,0,fileEntryMaxCount*sizeof(uint32_t));//set error len
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

inline static bool _writeUInt32(Zipper* self,uint32_t v){
    TByte buf[4];
    writeUInt32_to(buf,v);
    return _write(self,buf,4);
}
inline static bool _writeUInt16(Zipper* self,uint16_t v){
    TByte buf[2];
    writeUInt16_to(buf,v);
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

#define assert_align(self) assert((self->_curFilePos&(kZipAlignSize-1))==0);

static bool _write_fileHeaderInfo(Zipper* self,int fileIndex,UnZipper* srcZip,int srcFileIndex,bool isFullInfo){
    const TByte* headBuf=fileHeaderBuf(srcZip,srcFileIndex);
    bool isCompressed=UnZipper_file_isCompressed(srcZip,srcFileIndex);
    uint16_t fileNameLen=UnZipper_file_nameLen(srcZip,srcFileIndex);
    uint16_t extraFieldLen=readUInt16(headBuf+30);
    if (!isFullInfo){
        if (!isCompressed){//进行对齐;
            size_t headInfoLen=30+fileNameLen+extraFieldLen;
            size_t skipLen=_getAlignSkipLen(self->_curFilePos+headInfoLen);
            check(_writeAlignSkip(self,skipLen));
        }
        self->_fileEntryOffsets[fileIndex]=self->_curFilePos;
    }
    check(_writeUInt32(self,isFullInfo?kCENTRALHEADERMAGIC:kLOCALHEADERMAGIC));
    if (isFullInfo)
        check(_write(self,headBuf+4,2));//压缩版本;
    check(_write(self,headBuf+6,20-6));//解压缩版本--CRC-32校验;//其中 标志 中已经去掉了Data descriptor标识;
    check(_writeUInt32(self,self->_fileCompressedSizes[fileIndex]));//压缩后大小;
    check(_write(self,headBuf+24,30-24));//压缩前大小--文件名称长度;
    check(_writeUInt16(self,extraFieldLen));//扩展字段长度;
    
    uint16_t fileCommentLen=0;//文件注释长度;
    if (isFullInfo){
        fileCommentLen=readUInt16(headBuf+32);
        check(_writeUInt16(self,fileCommentLen));
        check(_write(self,headBuf+34,42-34));//文件开始的分卷号--文件外部属性;
        check(_writeUInt32(self,self->_fileEntryOffsets[fileIndex]));//对应文件在文件中的偏移;
    }
    check(_write(self,headBuf+46,fileNameLen+extraFieldLen+fileCommentLen));//文件名称 + 扩展字段 [+文件注释];
    if ((!isCompressed)&(!isFullInfo)) assert_align(self);//对齐检查;
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
    self->_fileEntryCount=curFileIndex+1;
    self->_fileCompressedSizes[curFileIndex]=(!isCompressed)?(uint32_t)dataSize:           //finally value
                                            UnZipper_file_compressedSize(srcZip,srcFileIndex);//temp value
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
    
    if (compressedSizeOffset>=self->_curFilePos-self->_curBufLen){//in cache
        TByte* buf=self->_buf+compressedSizeOffset-(self->_curFilePos-self->_curBufLen);
        writeUInt32_to(buf,compressedSize);
    }else{
        TByte buf[4];
        writeUInt32_to(buf,compressedSize);
        check(_writeFlush(self));
        hpatch_StreamPos_t backFilePos=0;
        check(fileTell64(self->_file,&backFilePos));
        check(fileSeek64(self->_file,compressedSizeOffset,SEEK_SET));
        check(fileWrite(self->_file,buf,buf+4));
        check(fileSeek64(self->_file,backFilePos,SEEK_SET));
    }
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
    check(self->_fileEntryCount==self->_fileHeaderCount);
    const TByte* endBuf=srcZip->_endCentralDirectory;
    uint32_t centralDirectory_size=self->_curFilePos-self->_centralDirectory_pos;
    
    check(_write(self,endBuf+0,10-0));//固定魔法值--当前分卷Central Directory的记录数量;
    check(_writeUInt16(self,self->_fileEntryCount));
    check(_writeUInt32(self,centralDirectory_size));
    check(_writeUInt32(self,self->_centralDirectory_pos));
    check(_writeUInt16(self,0));//Zip文件注释长度;
    return  _writeFlush(self);  
}

