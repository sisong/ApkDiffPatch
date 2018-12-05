//  Zipper.cpp
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
#include "../parallel/parallel.h"
#include "Zipper.h"
#include <string.h>
#include "../../HDiffPatch/file_for_patch.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff_types.h"
#include "patch_types.h"
#include "../parallel/parallel.h"
#include "../../HDiffPatch/compress_plugin_demo.h"
#include "../../HDiffPatch/decompress_plugin_demo.h"
static hdiff_TStreamCompress* compressPlugin  =&zlibStreamCompressPlugin;
static hpatch_TDecompress*    decompressPlugin=&zlibDecompressPlugin;

#define  kNormalizedZlibVersion         "1.2.11" //fixed zlib version

#define check(v) { if (!(v)) { assert(false); return false; } }

inline static uint16_t readUInt16(const TByte* buf){
    return buf[0]|(buf[1]<<8);
}
inline static hpatch_StreamPos_t readUInt64(const TByte* buf){
    hpatch_StreamPos_t h=readUInt32(buf+4);
    return readUInt32(buf) | (h<<32);
}

inline static void writeUInt16_to(TByte* out_buf2,uint32_t v){
    out_buf2[0]=(TByte)v; out_buf2[1]=(TByte)(v>>8);
}

typedef enum TDataDescriptor{
    kDataDescriptor_NO =0,
    kDataDescriptor_12 =1,
    kDataDescriptor_16 =2
} TDataDescriptor;

#define kBufSize                    (16*1024)

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
        check((long)readLen==self->stream->read(self->stream->streamHandle,fileLength-readed_pos,buf,buf+readLen));
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


bool UnZipper_searchApkV2Sign(const hpatch_TStreamInput* stream,hpatch_StreamPos_t centralDirectory_pos,
                              ZipFilePos_t* v2sign_pos,hpatch_StreamPos_t* out_blockSize){
    *v2sign_pos=(ZipFilePos_t)centralDirectory_pos; //default not found
    
    //tag
    const size_t APKSigningTagLen=16;
    const char* APKSigningTag="APK Sig Block 42";
    if (APKSigningTagLen>centralDirectory_pos) return true;
    ZipFilePos_t APKSigningBlockTagPos=(ZipFilePos_t)centralDirectory_pos-APKSigningTagLen;
    TByte buf[APKSigningTagLen];
    check((long)APKSigningTagLen==stream->read(stream->streamHandle,
                                               APKSigningBlockTagPos,buf,buf+APKSigningTagLen));
    if (0!=memcmp(buf,APKSigningTag,APKSigningTagLen)) return true;
    //bottom size
    if (8>APKSigningBlockTagPos) return false; //error
    ZipFilePos_t blockSizeBottomPos=APKSigningBlockTagPos-8;
    check(8==stream->read(stream->streamHandle,blockSizeBottomPos,buf,buf+8));
    hpatch_StreamPos_t blockSize=readUInt64(buf);
    //top
    if (blockSize+8>centralDirectory_pos) return false; //error
    ZipFilePos_t blockSizeTopPos=(ZipFilePos_t)centralDirectory_pos-(ZipFilePos_t)blockSize-8;
    check(8==stream->read(stream->streamHandle,blockSizeTopPos,buf,buf+8));
    check(blockSize==readUInt64(buf)); //check top size
    
    *v2sign_pos=blockSizeTopPos;
    *out_blockSize=blockSize;
    return true;
}

inline static bool _UnZipper_searchApkV2Sign(UnZipper* self,ZipFilePos_t centralDirectory_pos,
                                      ZipFilePos_t* v2sign_pos){
    hpatch_StreamPos_t blockSize=0;
    return UnZipper_searchApkV2Sign(self->stream,centralDirectory_pos,v2sign_pos,&blockSize);
}

inline static ZipFilePos_t _fileData_offset_read(UnZipper* self,ZipFilePos_t entryOffset){
    TByte buf[4];
    check(UnZipper_fileData_read(self,entryOffset+26,buf,buf+4));
    return entryOffset+30+readUInt16(buf)+readUInt16(buf+2);
}

int UnZipper_fileCount(const UnZipper* self){
    return readUInt16(self->_endCentralDirectory+8);
}
inline static int32_t _centralDirectory_size(const UnZipper* self){
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

bool UnZipper_isHaveApkV1_or_jarSign(const UnZipper* self){
    int fCount=UnZipper_fileCount(self);
    for (int i=fCount-1; i>=0; --i) {
        if (UnZipper_file_isApkV1_or_jarSign(self,i))
            return true;
    }
    return false;
}

bool UnZipper_file_isApkV1_or_jarSign(const UnZipper* self,int fileIndex){
    const char* kJarSignPath="META-INF/";
    const size_t kJarSignPathLen=8+1;
    return (UnZipper_file_nameLen(self,fileIndex)>=(int)kJarSignPathLen)
        && (0==memcmp(UnZipper_file_nameBegin(self,fileIndex),kJarSignPath,kJarSignPathLen));
}

bool UnZipper_file_isApkV2Compressed(const UnZipper* self,int fileIndex){
    return UnZipper_isHaveApkV2Sign(self)
                && UnZipper_file_isCompressed(self,fileIndex)
                && UnZipper_file_isApkV1_or_jarSign(self,fileIndex);
}

//缓存相关信息并规范化数据;
static bool _UnZipper_vce_normalized(UnZipper* self,bool isFileDataOffsetMatch){
    assert(self->_endCentralDirectory!=0);
    writeUInt32_to(self->_endCentralDirectory+16,0);//normalized CentralDirectory的开始位置偏移;
    self->_isFileDataOffsetMatch=isFileDataOffsetMatch;
    bool test_isFileDataOffsetMatch=true;
    
    assert(self->_centralDirectory!=0);
    TByte* buf=self->_centralDirectory;
    const int fileCount=UnZipper_fileCount(self);
    size_t centralDirectory_size=_centralDirectory_size(self);
    int curOffset=0;
    memset(self->_dataDescriptors,kDataDescriptor_NO,fileCount);
    self->_dataDescriptorCount=0;
    for (int i=0; i<fileCount; ++i) {
        TByte* headBuf=buf+curOffset;
        check(kCENTRALHEADERMAGIC==readUInt32(headBuf));
        self->_fileHeaderOffsets[i]=curOffset;
        
        self->_fileCompressedSizes[i]=readUInt32(headBuf+20);
        writeUInt32_to(headBuf+20,0);//normalized 压缩大小占位;
        
        uint32_t fileEntryOffset=readUInt32(headBuf+42);
        writeUInt32_to(headBuf+42,0);//normalized 文件Entry开始位置偏移;
        ZipFilePos_t fastFileDataOffset=(fileEntryOffset+30+readUInt16(headBuf+28)+readUInt16(headBuf+30)); //fast
        if (isFileDataOffsetMatch){
            self->_fileDataOffsets[i]=fastFileDataOffset;
        }else{
            ZipFilePos_t fileDataOffset=_fileData_offset_read(self,fileEntryOffset);//seek&read;
            self->_fileDataOffsets[i]=fileDataOffset;
            test_isFileDataOffsetMatch&=(fileDataOffset==fastFileDataOffset);
        }
        
        uint16_t fileTag=readUInt16(headBuf+8);//标志;
        if ((fileTag&(1<<3))!=0){//have descriptor?
            writeUInt16_to(headBuf+8,fileTag&(~(1<<3)));//normalized 标志中去掉Data descriptor标识;
            uint32_t crc=UnZipper_file_crc32(self,i);
            uint32_t compressedSize=UnZipper_file_compressedSize(self,i);
            TByte buf[16];
            check(UnZipper_fileData_read(self,self->_fileDataOffsets[i]+self->_fileCompressedSizes[i],buf,buf+16));
            if ((readUInt32(buf)==crc)&&(readUInt32(buf+4)==compressedSize))
                self->_dataDescriptors[i]=kDataDescriptor_12;
            else if ((readUInt32(buf+4)==crc)&&(readUInt32(buf+8)==compressedSize))
                self->_dataDescriptors[i]=kDataDescriptor_16;
            else
                check(false);
            ++self->_dataDescriptorCount;
        }
        
        int fileNameLen=UnZipper_file_nameLen(self,i);
        int extraFieldLen=readUInt16(headBuf+30);
        int fileCommentLen=readUInt16(headBuf+32);
        curOffset+= kMinFileHeaderSize + fileNameLen+extraFieldLen+fileCommentLen;
        check((size_t)curOffset <= centralDirectory_size);
    }
    
    //update
    self->_isFileDataOffsetMatch=test_isFileDataOffsetMatch;
    
    return true;
}

void UnZipper_init(UnZipper* self){
    memset(self,0,sizeof(*self));
}
bool UnZipper_close(UnZipper* self){
    self->stream=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    if (self->_cache_vce) { free(self->_cache_vce); self->_cache_vce=0; }
    if (self->_fileStream.m_file!=0) { check(TFileStreamInput_close(&self->_fileStream)); }
    return true;
}

static bool _UnZipper_open_begin(UnZipper* self){
    assert(self->_buf==0);
    self->_buf=(unsigned char*)malloc(kBufSize);
    check(self->_buf!=0);
    self->_isDataNormalized=false;
    return true;
}

static bool _UnZipper_open_vce(UnZipper* self,ZipFilePos_t vceSize,int fileCount){
    assert(self->_cache_vce==0);
    self->_vce_size=vceSize;
    self->_cache_vce=(TByte*)malloc(self->_vce_size+1*fileCount
                                    +sizeof(ZipFilePos_t)*(fileCount+1)+sizeof(uint32_t)*fileCount*2);
    check(self->_cache_vce!=0);
    self->_dataDescriptors=self->_cache_vce+self->_vce_size;
    size_t alignBuf=_hpatch_align_upper((self->_dataDescriptors+fileCount),sizeof(ZipFilePos_t));
    self->_fileDataOffsets=(ZipFilePos_t*)alignBuf;
    self->_fileHeaderOffsets=(uint32_t*)(self->_fileDataOffsets+fileCount);
    self->_fileCompressedSizes=(uint32_t*)(self->_fileHeaderOffsets+fileCount);
    return true;
}

#define _UnZipper_sreachVCE() \
    ZipFilePos_t endCentralDirectory_pos=0; \
    ZipFilePos_t centralDirectory_pos=0;    \
    uint32_t     fileCount=0;   \
    ZipFilePos_t v2sign_pos=0;  \
    check(_UnZipper_searchEndCentralDirectory(self,&endCentralDirectory_pos));  \
    check(_UnZipper_searchCentralDirectory(self,endCentralDirectory_pos,&centralDirectory_pos,&fileCount)); \
    check(_UnZipper_searchApkV2Sign(self,centralDirectory_pos,&v2sign_pos));


bool UnZipper_openStream(UnZipper* self,const hpatch_TStreamInput* zipFileStream,
                         bool isDataNormalized,bool isFileDataOffsetMatch){
    check(_UnZipper_open_begin(self));
    self->stream=zipFileStream;
    _UnZipper_sreachVCE();
    check(_UnZipper_open_vce(self,(ZipFilePos_t)self->stream->streamSize-v2sign_pos,fileCount));
    self->_centralDirectory=self->_cache_vce+(centralDirectory_pos-v2sign_pos);
    self->_endCentralDirectory=self->_cache_vce+(endCentralDirectory_pos-v2sign_pos);
    
    check(UnZipper_fileData_read(self,v2sign_pos,self->_cache_vce,self->_cache_vce+self->_vce_size));
    
    self->_isDataNormalized=isDataNormalized;
    self->_isFileDataOffsetMatch=isFileDataOffsetMatch;
    check(_UnZipper_vce_normalized(self,isFileDataOffsetMatch));
    return true;
}

bool UnZipper_openFile(UnZipper* self,const char* zipFileName,bool isDataNormalized,bool isFileDataOffsetMatch){
    check(TFileStreamInput_open(&self->_fileStream,zipFileName));
    return UnZipper_openStream(self,&self->_fileStream.base,isDataNormalized,isFileDataOffsetMatch);
}

bool UnZipper_openVCE(UnZipper* self,ZipFilePos_t vce_size,int fileCount){
    check(_UnZipper_open_begin(self));
    check(_UnZipper_open_vce(self,vce_size,fileCount));
    mem_as_hStreamInput(&self->_fileStream.base,self->_cache_vce,self->_cache_vce+self->_vce_size);
    self->stream=&self->_fileStream.base;
    return true;
}

bool UnZipper_updateVCE(UnZipper* self,bool isDataNormalized,size_t zipCESize){
    ZipFilePos_t endCentralDirectory_pos=0;
    ZipFilePos_t centralDirectory_pos=(ZipFilePos_t)(self->_vce_size-zipCESize);
    ZipFilePos_t v2sign_pos=0;
    check(_UnZipper_searchEndCentralDirectory(self,&endCentralDirectory_pos));

    self->_centralDirectory=self->_cache_vce+(centralDirectory_pos-v2sign_pos);
    self->_endCentralDirectory=self->_cache_vce+(endCentralDirectory_pos-v2sign_pos);
    self->_isDataNormalized=isDataNormalized;
    
    bool isFileDataOffsetMatch=true;
    check(_UnZipper_vce_normalized(self,isFileDataOffsetMatch));
    return true;
}


inline static uint16_t _file_compressType(const UnZipper* self,int fileIndex){
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

ZipFilePos_t UnZipper_fileEntry_endOffset(const UnZipper* self,int fileIndex){
    ZipFilePos_t result=UnZipper_fileData_offset(self,fileIndex)
                        +UnZipper_file_compressedSize(self,fileIndex);
    TDataDescriptor desc=(TDataDescriptor)self->_dataDescriptors[fileIndex];
    if (desc==kDataDescriptor_12)
        result+=12;
    else if (desc==kDataDescriptor_16)
        result+=16;
    return result;
}

ZipFilePos_t UnZipper_fileData_offset(const UnZipper* self,int fileIndex){
    return self->_fileDataOffsets[fileIndex];
}

ZipFilePos_t UnZipper_fileEntry_offset_unsafe(const UnZipper* self,int fileIndex){
    //_UnZipper_vce_normalized set fileEntry offset 0
    const TByte* headBuf=fileHeaderBuf(self,fileIndex);
    return self->_fileDataOffsets[fileIndex]-30-readUInt16(headBuf+28)-readUInt16(headBuf+30);
}

bool UnZipper_fileData_read(UnZipper* self,ZipFilePos_t file_pos,unsigned char* buf,unsigned char* bufEnd){
    //当前的实现不支持多线程;
    assert(self->_fileStream.m_file!=0);
    return (long)(bufEnd-buf)==self->stream->read(self->stream->streamHandle,file_pos,buf,bufEnd);
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

#define check_clear(v) { if (!(v)) { result=false; assert(false); goto clear; } }

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
    
    _zlib_TDecompress* decHandle=_zlib_decompress_open_by(decompressPlugin,self->stream,file_offset,
                                                          file_offset+file_compressedSize,0,
                                                          self->_buf,(kBufSize>>1));
    TByte* dataBuf=self->_buf+(kBufSize>>1);
    ZipFilePos_t  curWritePos=0;
    check_clear(decHandle!=0);
    while (curWritePos<file_data_size){
        long readLen=(kBufSize>>1);
        if ((ZipFilePos_t)readLen>(file_data_size-curWritePos)) readLen=(long)(file_data_size-curWritePos);
        check_clear(readLen==_zlib_decompress_part(decompressPlugin,decHandle,dataBuf,dataBuf+readLen));
        check_clear(readLen==outStream->write(outStream->streamHandle,writeToPos+curWritePos,dataBuf,dataBuf+readLen));
        curWritePos+=readLen;
    }
    check_clear(_zlib_is_decompress_finish(decompressPlugin,decHandle));
clear:
    _zlib_decompress_close_by(decompressPlugin,decHandle);
    return result;
}

#if (IS_USED_MULTITHREAD)
//----
struct TZipThreadWork {
    TZipThreadWork* _next;
    TByte*  inputData;
    TByte*  code;
    size_t  inputDataSize;
    size_t  codeSize;
    size_t  writePos;
    int     compressLevel;
    int     compressMemLevel;
};

TZipThreadWork* newThreadWork(size_t _inputDataSize,size_t _codeSize,size_t _writePos,
                              int _compressLevel,int _compressMemLevel){
    TZipThreadWork* self=(TZipThreadWork*)malloc(sizeof(TZipThreadWork)+_inputDataSize+_codeSize);
    if (self==0) return 0;// error;
    self->_next=0;
    self->inputData=(TByte*)self + sizeof(TZipThreadWork);
    self->inputDataSize=_inputDataSize;
    self->code=self->inputData+_inputDataSize;
    self->codeSize=_codeSize;
    self->writePos=_writePos;
    self->compressLevel=_compressLevel;
    self->compressMemLevel=_compressMemLevel;
    return self;
}

inline void freeThreadWork(TZipThreadWork* self){
    if (self) free(self);
}

struct TZipThreadWorks{
//for thread
public:
    static void run(int threadIndex,void* workData){
        TZipThreadWorks* self=(TZipThreadWorks*)workData;
        while (TZipThreadWork* work=self->getWork()){
            self->doWork(work);
            self->endWork(work);
        }
    }
private:
    TZipThreadWork* getWork(){//wait,return 0 exit thread
        while (true){
            {   CAutoLoopLocker _locker(this->_locker);
                if (_waitingList!=0){
                    TZipThreadWork* result=(TZipThreadWork*)_waitingList;
                    _waitingList=_waitingList->_next;
                    if (_waitingList==0) _waitingList_last=0;
                    result->_next=0; //get a work
                    return result;
                }else if(_isAllWorkDispatched){
                    --_curWorkThreadNum;
                    return 0; //exit thread
                }
            }
            this_thread_yield();
        }
    }
    inline static void doWork(TZipThreadWork* work){
        size_t rsize=Zipper_compressData(work->inputData,work->inputDataSize,work->code,work->codeSize,
                                         work->compressLevel,work->compressMemLevel);
        if ((rsize==0)||(rsize!=work->codeSize))
            work->codeSize=0; //error or code size overflow
    }
    void endWork(TZipThreadWork* workResult){
        assert(workResult->_next==0);
        CAutoLoopLocker _locker(this->_locker);
        if (_finishList_last==0){
            _finishList=workResult;
            _finishList_last=workResult;
        }else{
            _finishList_last->_next=workResult;
            _finishList_last=workResult;
        }
    }
    
public:
    explicit TZipThreadWorks(Zipper* zip,int workThreadNum)
    :_zip(zip),_curWorkThreadNum(workThreadNum),_curWorkCount(0),_locker(0),
    _waitingList(0),_waitingList_last(0),_finishList(0),_finishList_last(0),
    _isAllWorkDispatched(false){
        _locker=locker_new();
    }
    ~TZipThreadWorks(){
        waitThreadsExit();
        locker_delete(_locker);
        freeList((TZipThreadWork*)_waitingList);
        freeList((TZipThreadWork*)_finishList);
    }
    
    //
    void dispatchWork(TZipThreadWork* newWork){
        assert(newWork->_next==0);
        CAutoLoopLocker _locker(this->_locker);
        if (_waitingList_last==0){
            _waitingList=newWork;
            _waitingList_last=newWork;
        }else{
            _waitingList_last->_next=newWork;
            _waitingList_last=newWork;
        }
    }
    void finishDispatchWork(){
        {   CAutoLoopLocker _locker(this->_locker);
            _isAllWorkDispatched=true;
        }
    }
    TZipThreadWork* haveFinishWork(bool isWait){
        while (true){
            {   CAutoLoopLocker _locker(this->_locker);
                if (_finishList!=0){
                    TZipThreadWork* result=(TZipThreadWork*)_finishList;
                    _finishList=_finishList->_next;
                    if (_finishList==0) _finishList_last=0;
                    result->_next=0; //get a finished work
                    return result;
                }else{
                    if (!isWait) return 0;//not need wait
                    if (_curWorkThreadNum==0) return 0;//all thread exit
                    //else wait
                }
            }
            this_thread_yield();
        }
    }
private:
    Zipper*             _zip;
    volatile int        _curWorkThreadNum;
    volatile int        _curWorkCount;
    
    HLocker             _locker;
    volatile TZipThreadWork*  _waitingList;
    volatile TZipThreadWork*  _waitingList_last;
    volatile TZipThreadWork*  _finishList;
    volatile TZipThreadWork*  _finishList_last;
    volatile bool       _isAllWorkDispatched;
    
    void waitThreadsExit(){
        while (true){
            {   CAutoLoopLocker _locker(this->_locker);
                if (_curWorkThreadNum==0) return;
                _isAllWorkDispatched=true;
            }
            this_thread_yield();
        }
    }
    void freeList(TZipThreadWork* list){
        while(list){
            TZipThreadWork* cur=list;
            list=list->_next;
            freeThreadWork(cur);
        }
    }
};
#else
    struct TZipThreadWork{};
    struct TZipThreadWorks{};
#endif //IS_USED_MULTITHREAD


void Zipper_init(Zipper* self){
    memset(self,0,sizeof(*self));
}

bool Zipper_close(Zipper* self){
#if (IS_USED_MULTITHREAD)
    if (self->_append_stream.threadWork){
        freeThreadWork(self->_append_stream.threadWork);
        self->_append_stream.threadWork=0;
    }
    if (self->_threadWorks){
        delete self->_threadWorks;
        self->_threadWorks=0;
    }
#endif
    
    self->_stream=0;
    self->_fileEntryCount=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    if (self->_fileStream.m_file) { check(TFileStreamOutput_close(&self->_fileStream)); }
    if (self->_append_stream.compressHandle!=0){
        struct _zlib_TCompress* compressHandle=self->_append_stream.compressHandle;
        self->_append_stream.compressHandle=0;
        check(_zlib_compress_close_by(compressPlugin,compressHandle));
    }
    return true;
}

#define checkCompressSet(compressLevel,compressMemLevel){   \
    check((Z_BEST_SPEED<=compressLevel)&&(compressLevel<=Z_BEST_COMPRESSION));  \
    check((1<=compressMemLevel)&&(compressMemLevel<=MAX_MEM_LEVEL)); }

#define isUsedMT(self) (self->_threadNum>1)

bool Zipper_openStream(Zipper* self,const hpatch_TStreamOutput* zipStream,int fileEntryMaxCount,
                       int ZipAlignSize,int compressLevel,int compressMemLevel){
    check(0==strcmp(kNormalizedZlibVersion,zlibVersion()));//fiexd zlib version
    assert(ZipAlignSize>0);
    if (ZipAlignSize<=0) ZipAlignSize=1;
    checkCompressSet(compressLevel,compressMemLevel);
    self->_ZipAlignSize=ZipAlignSize;
    self->_compressLevel=compressLevel;
    self->_compressMemLevel=compressMemLevel;
    
    assert(self->_stream==0);
    self->_stream=zipStream;
    
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

void Zipper_by_multi_thread(Zipper* self,int threadNum){
    assert(self->_fileEntryCount==0);
    assert(self->_threadNum<=1);
    assert(self->_threadWorks==0);
#if (IS_USED_MULTITHREAD)
    threadNum=(threadNum<=self->_fileEntryMaxCount)?threadNum:self->_fileEntryMaxCount;
    self->_threadNum=(threadNum>=1)?threadNum:1;
    if (isUsedMT(self)){
        try {
            self->_threadWorks=new TZipThreadWorks(self,self->_threadNum-1);
            thread_parallel(self->_threadNum-1,TZipThreadWorks::run,self->_threadWorks,false);
        } catch (...) {
            if (self->_threadWorks) delete self->_threadWorks;
            self->_threadWorks=0;
            self->_threadNum=1;
        }
    }
#else
    self->_threadNum=1;
#endif
}

bool Zipper_openFile(Zipper* self,const char* zipFileName,int fileEntryMaxCount,
                      int ZipAlignSize,int compressLevel,int compressMemLevel){
    assert(self->_fileStream.m_file==0);
    check(TFileStreamOutput_open(&self->_fileStream,zipFileName,(hpatch_StreamPos_t)(-1)));
    TFileStreamOutput_setRandomOut(&self->_fileStream,hpatch_TRUE);
    return Zipper_openStream(self,&self->_fileStream.base,fileEntryMaxCount,
                             ZipAlignSize,compressLevel,compressMemLevel);
}

static bool _writeFlush(Zipper* self){
    size_t curBufLen=self->_curBufLen;
    if (curBufLen>0){
        self->_curBufLen=0;
        check((long)curBufLen==self->_stream->write(self->_stream->streamHandle,self->_curFilePos-curBufLen,
                                                    self->_buf,self->_buf+curBufLen));
    }
    return true;
}
static bool _write(Zipper* self,const TByte* data,size_t len){
    size_t curBufLen=self->_curBufLen;
    if (((curBufLen>0)||(len*2<=kBufSize)) && (curBufLen+len<=kBufSize)){//to buf
        memcpy(self->_buf+curBufLen,data,len);
        self->_curBufLen=curBufLen+len;
    }else{
        if (curBufLen>0)
            check(_writeFlush(self));
        check((long)len==self->_stream->write(self->_stream->streamHandle,
                                              self->_curFilePos-self->_curBufLen,data,data+len));
    }
    self->_curFilePos+=(ZipFilePos_t)len;
    return true;
}

inline static bool _writeSkip(Zipper* self,size_t skipLen){
    check(_writeFlush(self));
    self->_curFilePos+=(ZipFilePos_t)skipLen;
    return true;
}

inline static bool _writeBack(Zipper* self,size_t backPos,const TByte* data,size_t len){
    check((long)len==self->_stream->write(self->_stream->streamHandle,backPos,data,data+len));
    return true;
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


inline static size_t _getAlignSkipLen(const Zipper* self,size_t curPos){
    size_t align=self->_ZipAlignSize;
    return align-1-(curPos+align-1)%align;
}
inline static bool _writeAlignSkip(Zipper* self,size_t alignSkipLen){
    assert(alignSkipLen<self->_ZipAlignSize);
    const size_t bufSize =16;
    const TByte _alignSkipBuf[bufSize]={0};
    while (alignSkipLen>0) {
        size_t wlen=alignSkipLen;
        if (wlen>bufSize) wlen=bufSize;
        check(_write(self,_alignSkipBuf,wlen));
        alignSkipLen-=wlen;
    }
    return true;
}

#define assert_align(self) assert((self->_curFilePos%self->_ZipAlignSize)==0)

static bool _write_fileHeaderInfo(Zipper* self,int fileIndex,UnZipper* srcZip,int srcFileIndex,bool isFullInfo){
    const TByte* headBuf=fileHeaderBuf(srcZip,srcFileIndex);
    bool isCompressed=UnZipper_file_isCompressed(srcZip,srcFileIndex);
    uint32_t compressedSize=self->_fileCompressedSizes[fileIndex];
    uint16_t fileNameLen=UnZipper_file_nameLen(srcZip,srcFileIndex);
    uint16_t extraFieldLen=readUInt16(headBuf+30);
    bool isNeedAlign=(!isFullInfo)&&(!isCompressed); //dir or 0 size file need align too, same AndroidSDK#zipalign
    if (isNeedAlign){
        size_t headInfoLen=30+fileNameLen+extraFieldLen;
        size_t skipLen=_getAlignSkipLen(self,self->_curFilePos+headInfoLen);
        check(_writeAlignSkip(self,skipLen));
    }
    if (!isFullInfo){
        self->_fileEntryOffsets[fileIndex]=self->_curFilePos;
    }
    check(_writeUInt32(self,isFullInfo?kCENTRALHEADERMAGIC:kLOCALHEADERMAGIC));
    if (isFullInfo)
        check(_write(self,headBuf+4,2));//压缩版本;
    check(_write(self,headBuf+6,20-6));//解压缩版本--CRC-32校验;//其中 标志 中已经去掉了Data descriptor标识;
    check(_writeUInt32(self,compressedSize));//压缩后大小;
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
    if (isNeedAlign) assert_align(self);//对齐检查;
    return  true;
}


size_t Zipper_compressData_maxCodeSize(size_t dataSize){
    return _zlib_maxCompressedSize(&zlibCompressPlugin,dataSize);
}

size_t Zipper_compressData(const unsigned char* data,size_t dataSize,unsigned char* out_code,
                           size_t codeSize,int kCompressLevel,int kCompressMemLevel){
    const int kCodeBufSize=1024;
    TByte codeBuf[kCodeBufSize];
    hdiff_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,out_code,out_code+codeSize);
    _zlib_TCompress* compressHandle=_zlib_compress_open_by(compressPlugin,&stream,0,kCompressLevel,
                                                           kCompressMemLevel,codeBuf,kCodeBufSize);
    if (compressHandle==0) return 0;//error;
    int outStream_isCanceled=0;//false
    hpatch_StreamPos_t curWritedPos=0;
    int is_data_end=true;
    if (!_zlib_compress_stream_part(compressPlugin,compressHandle,data,data+dataSize,
                                    is_data_end,&curWritedPos,&outStream_isCanceled)) return 0;//error
    if (!_zlib_compress_close_by(compressPlugin,compressHandle)) return 0;//error
    return (size_t)curWritedPos;
}

long Zipper_file_append_stream::_append_part_input(hpatch_TStreamOutputHandle handle,hpatch_StreamPos_t pos,
                                                   const unsigned char *part_data,
                                                   const unsigned char *part_data_end){
    Zipper_file_append_stream* append_state=(Zipper_file_append_stream*)handle;
    assert(append_state->inputPos==pos);
    long partSize=(long)(part_data_end-part_data);
    append_state->inputPos+=partSize;
    if (append_state->inputPos>append_state->streamSize) return false;
    int is_data_end=(append_state->inputPos==append_state->streamSize);
#if (IS_USED_MULTITHREAD)
    if (append_state->threadWork){
        memcpy(append_state->threadWork->inputData+append_state->inputPos-partSize,part_data,partSize);
        return partSize;
    } else
#endif
    if (append_state->compressHandle){
        int outStream_isCanceled=0;//false
        hpatch_StreamPos_t curWritedPos=append_state->outputPos;
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
                              bool dataIsCompressed,size_t dataUncompressedSize,size_t dataCompressedSize){
    return Zipper_file_append_beginWith(self,srcZip,srcFileIndex,dataIsCompressed,dataUncompressedSize,
                                        dataCompressedSize,self->_compressLevel,self->_compressMemLevel);
}
bool Zipper_file_append_beginWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                  bool dataIsCompressed,size_t dataUncompressedSize,size_t dataCompressedSize,
                                  int curFileCompressLevel,int curFileCompressMemLevel){
    const bool isCompressed=UnZipper_file_isCompressed(srcZip,srcFileIndex);
    if (isCompressed&&(!dataIsCompressed))
        checkCompressSet(curFileCompressLevel,curFileCompressMemLevel);
    if ((!isCompressed)&&(dataIsCompressed)){
        assert(false);  //now need input decompressed data;
        return false;       // for example: UnZipper_fileData_decompressTo(Zipper_file_append_part_as_stream());
    }
    if (0==dataCompressedSize){
        check(!dataIsCompressed);
        dataCompressedSize=UnZipper_file_compressedSize(srcZip,srcFileIndex);//temp value
    }
    Zipper_file_append_stream* append_state=&self->_append_stream;
    if (append_state->self!=0){
        assert(false);  //need call Zipper_file_append_end()
        return false;
    }
    
    int curFileIndex=self->_fileEntryCount;
    check(curFileIndex < self->_fileEntryMaxCount);
    self->_fileEntryCount=curFileIndex+1;
    self->_fileCompressedSizes[curFileIndex]=(isCompressed)?(uint32_t)dataCompressedSize: //maybe temp value
                                            (uint32_t)dataUncompressedSize; //finally value
    check(_write_fileHeaderInfo(self,curFileIndex,srcZip,srcFileIndex,false));//out file info
    
    append_state->self=self;
    append_state->inputPos=0;
    append_state->outputPos=0;
    append_state->curFileIndex=curFileIndex;
    append_state->streamHandle=append_state;
    append_state->streamSize=dataIsCompressed?dataCompressedSize:dataUncompressedSize; //finally value
    append_state->write=Zipper_file_append_stream::_append_part_input;
    assert(append_state->compressHandle==0);
    assert(append_state->threadWork==0);
    if (isCompressed&&(!dataIsCompressed)){//compress data
#if (IS_USED_MULTITHREAD)
        if (isUsedMT(self)){
            append_state->threadWork=newThreadWork(dataUncompressedSize,dataCompressedSize,self->_curFilePos,
                                                   curFileCompressLevel,curFileCompressMemLevel);
            if ((append_state->threadWork)&&(!_writeSkip(self,dataCompressedSize)))
                return false;
        }
#endif
        if (!append_state->threadWork){
            append_state->compressOutStream.streamHandle=append_state;
            append_state->compressOutStream.streamSize=self->_fileCompressedSizes[curFileIndex];//not used
            append_state->compressOutStream.write=Zipper_file_append_stream::_append_part_output;
            append_state->compressHandle=_zlib_compress_open_by(compressPlugin,&append_state->compressOutStream,
                                                                0,curFileCompressLevel,curFileCompressMemLevel,
                                                                self->_codeBuf,kBufSize);
            if (!append_state->compressHandle)
                return false;
        }
    }//else //copy data
    return true;
}

bool _zipper_file_update_compressedSize(Zipper* self,int curFileIndex,uint32_t compressedSize){
    check(curFileIndex<self->_fileEntryCount);
    if (self->_fileCompressedSizes[curFileIndex]==compressedSize) return true;
    self->_fileCompressedSizes[curFileIndex]=compressedSize;
    
    uint32_t fileEntryOffset=self->_fileEntryOffsets[curFileIndex];
    uint32_t compressedSizeOffset=fileEntryOffset+18;
    
    if (compressedSizeOffset>=self->_curFilePos-self->_curBufLen){//all in cache
        TByte* buf=self->_buf+compressedSizeOffset-(self->_curFilePos-self->_curBufLen);
        writeUInt32_to(buf,compressedSize);
    }else{
        TByte buf[4];
        writeUInt32_to(buf,compressedSize);
        if (compressedSizeOffset+4>self->_curFilePos-self->_curBufLen){
            check(_writeFlush(self));
        }
        check(_writeBack(self,compressedSizeOffset,buf,4));
    }
    return true;
}

#if (IS_USED_MULTITHREAD)
static bool dispose_filishedThreadWork(Zipper* self,bool isWait){
    while (true) {
        TZipThreadWork* work=self->_threadWorks->haveFinishWork(isWait);
        if (work){
            check(_writeBack(self,work->writePos,work->code,work->codeSize));
            freeThreadWork(work);
        }else{
            return true;
        }
    }
}
#endif

bool Zipper_file_append_end(Zipper* self){
    Zipper_file_append_stream* append_state=&self->_append_stream;
    if (append_state->self==0) { assert(false); return false; }
    
    bool result=true;
    if (append_state->compressHandle!=0){
        check_clear(_zlib_compress_close_by(compressPlugin,append_state->compressHandle));
    }
    
    check_clear(append_state->inputPos==append_state->streamSize);
#if (IS_USED_MULTITHREAD)
    if (append_state->threadWork){
        self->_threadWorks->dispatchWork(append_state->threadWork);
        append_state->threadWork=0;
    }
    if (isUsedMT(self))
        check_clear(dispose_filishedThreadWork(self,false));
#endif
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
    return ((long)partSize==append_state->write(append_state->streamHandle,append_state->inputPos,
                                                part_data,part_data+partSize));
}

bool Zipper_file_append_copy(Zipper* self,UnZipper* srcZip,int srcFileIndex,bool isAlwaysReCompress){
    bool dataIsCompressed=isAlwaysReCompress?false:UnZipper_file_isCompressed(srcZip,srcFileIndex);
    check(Zipper_file_append_begin(self,srcZip,srcFileIndex,dataIsCompressed,
                                   UnZipper_file_uncompressedSize(srcZip,srcFileIndex),
                                   UnZipper_file_compressedSize(srcZip,srcFileIndex)));
    if (isAlwaysReCompress){
        check(UnZipper_fileData_decompressTo(srcZip,srcFileIndex,&self->_append_stream));
    }else{
        check(UnZipper_fileData_copyTo(srcZip,srcFileIndex,&self->_append_stream));
    }
    check(Zipper_file_append_end(self));
    return true;
}

bool Zipper_copyExtra_before_fileHeader(Zipper* self,UnZipper* srcZip){
    if (srcZip->_cache_vce == srcZip->_centralDirectory)
        return true;
    return _write(self,srcZip->_cache_vce,srcZip->_centralDirectory-srcZip->_cache_vce);
}

bool Zipper_fileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex){
    int curFileIndex=self->_fileHeaderCount;
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
    
    check(_write(self,endBuf+0,8-0));//固定魔法值--Central Directory的开始分卷号;
    check(_writeUInt16(self,self->_fileEntryCount));
    check(_writeUInt16(self,self->_fileEntryCount));
    check(_writeUInt32(self,centralDirectory_size));
    check(_writeUInt32(self,self->_centralDirectory_pos));
    uint16_t endCommentLen=readUInt16(srcZip->_endCentralDirectory+20); //Zip文件注释长度;
    check(_writeUInt16(self,endCommentLen));
    check(_write(self,srcZip->_endCentralDirectory+22,endCommentLen));//Zip文件注释;
    check(_writeFlush(self));
#if (IS_USED_MULTITHREAD)
    if (isUsedMT(self)){
        self->_threadWorks->finishDispatchWork();
        check(dispose_filishedThreadWork(self,true));
    }
#endif
    return true;
}

