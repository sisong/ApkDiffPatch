//
//  Zipper.cpp
//  ApkPatch
//
//  Created by housisong on 2018/2/25.
//  Copyright © 2018年 housisong. All rights reserved.
//

#include "Zipper.h"
#include "../../HDiffPatch/file_for_patch.h"
//https://en.wikipedia.org/wiki/Zip_(file_format)

#define check(v) { if (!(v)) { assert(false); return false; } }

inline static uint16_t readUInt16(const TByte* buf){
    return buf[0]|(buf[1]<<8);
}
inline static uint32_t readUInt32(const TByte* buf){
    return buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);
}

#define kBufSize                    (16*1024)

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
    return true;
}

bool  UnZipper_file_isCompressed(const UnZipper* self,int fileIndex){
    uint16_t compressType=readUInt16(fileHeaderBuf(self,fileIndex)+10);
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

bool UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,
                              void* dstHandle,UnZipper_fileData_callback callback){
    TByte* buf=self->_buf;
    ZipFilePos_t fileSavedSize=UnZipper_file_compressedSize(self,fileIndex);
    ZipFilePos_t fileDataOffset=UnZipper_fileData_offset(self,fileIndex);
    ZipFilePos_t writedLen=0;
    while (writedLen<fileSavedSize) {
        ZipFilePos_t readLen=fileSavedSize-writedLen;
        if (readLen>kBufSize) readLen=kBufSize;
        check(UnZipper_fileData_read(self,fileDataOffset+writedLen,buf,buf+readLen));
        check(callback(dstHandle,buf,buf+readLen));
        writedLen+=readLen;
    }
    return true;
}

bool UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,
                                    void* dstHandle,UnZipper_fileData_callback callback){
    return false;
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
    if (!fileOpenForCreateOrReWrite(zipFileName,&self->_file)) return false;
    TByte* buf=(TByte*)malloc(kBufSize+sizeof(ZipFilePos_t)*(fileEntryMaxCount+1)+fileEntryMaxCount*sizeof(TByte));
    self->_buf=buf;
    size_t alignBuf=_hpatch_align_upper((buf+kBufSize),sizeof(ZipFilePos_t));
    self->_fileEntryOffsets=(ZipFilePos_t*)alignBuf;
    self->_extFieldLens=(TByte*)(self->_fileEntryOffsets+fileEntryMaxCount);
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
static bool _Zipper_write(void* self,const TByte* data,const TByte* dataEnd){
    return _write((Zipper*)self,data,dataEnd-data);
}

inline static bool _writeUInt32(Zipper* self,uint32_t v){
    TByte buf[4];
    buf[0]=(TByte)v; buf[1]=(TByte)(v>>8);
    buf[2]=(TByte)(v>>16); buf[3]=(TByte)(v>>24);
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
    assert_align(self);
    const TByte* headBuf=fileHeaderBuf(srcZip,srcFileIndex);
    check(_writeUInt32(self,isFullInfo?kCENTRALHEADERMAGIC:kLOCALHEADERMAGIC));
    if (isFullInfo)
        check(_write(self,headBuf+4,2));//压缩版本;
    check(_write(self,headBuf+6,2));//解压缩版本;
    uint16_t fileTag=readUInt16(headBuf+8);//标志;
    check(_writeUInt16(self,fileTag&(~(1<<3))));//标志中去掉Data descriptor标识;
    check(_write(self,headBuf+10,30-10));//压缩方式--文件名称长度;
    
    uint16_t fileNameLen=UnZipper_file_nameLen(srcZip,srcFileIndex);
    
    uint16_t extFieldLen=0;
    if (!isFullInfo){
        //利用 扩展字段 对齐;
        extFieldLen=_getAlignSkipLen(self->_curFilePos+2+fileNameLen);
        self->_extFieldLens[fileIndex]=(TByte)extFieldLen;
    }else{
        //已经设置过了 扩展字段 的长度;
        extFieldLen=self->_extFieldLens[fileIndex];
    }
    check(_writeUInt16(self,extFieldLen));//扩展字段长度;
    
    uint16_t fileCommentLen=0;
    if (isFullInfo){
        fileCommentLen=_getAlignSkipLen(self->_curFilePos+2+(42-34)+4+fileNameLen+extFieldLen);//利用 文件注释 对齐;
        check(_writeUInt16(self,fileCommentLen));//文件注释长度;
        check(_write(self,headBuf+34,42-34));//文件开始的分卷号--文件外部属性;
        check(_writeUInt32(self,self->_fileEntryOffsets[fileIndex]));//对应文件实体在文件中的偏移;
    }
    
    check(_write(self,UnZipper_file_nameBegin(srcZip,srcFileIndex),fileNameLen));//文件名;
    if (extFieldLen>0)
        check(_writeAlignSkip(self,extFieldLen));
    if (fileCommentLen>0)
        check(_writeAlignSkip(self,fileCommentLen));
    assert_align(self);
    return  true;
}


bool Zipper_file_append(Zipper* self,UnZipper* srcZip,int srcFileIndex){
    ZipFilePos_t curFileIndex=self->_fileEntryCount;
    check(curFileIndex < self->_fileEntryMaxCount);
    self->_fileEntryOffsets[curFileIndex]=self->_curFilePos;
    self->_fileEntryCount=curFileIndex+1;
    
    check(_write_fileHeaderInfo(self,curFileIndex,srcZip,srcFileIndex,false));
    check(UnZipper_fileData_copyTo(srcZip,srcFileIndex,self,_Zipper_write));//copy文件数据
    check(_writeToAlign(self));//填充数据对齐;
    return  true;
}

bool Zipper_file_appendWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                 const unsigned char* data,size_t dataSize,bool isNeedCompress){
    //todo:
    return  false;
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
    assert_align(self);
    check(self->_fileEntryCount==self->_fileHeaderCount);
    const TByte* endBuf=srcZip->_endCentralDirectoryInfo;
    uint32_t centralDirectory_size=self->_curFilePos-self->_centralDirectory_pos;
    
    check(_write(self,endBuf+0,10-0));//固定魔法值--当前分卷Central Directory的记录数量;
    check(_writeUInt16(self,self->_fileEntryCount));
    check(_writeUInt32(self,centralDirectory_size));
    check(_writeUInt32(self,self->_centralDirectory_pos));
    
    uint16_t zipCommentLen=_getAlignSkipLen(self->_curFilePos+2);//利用 Zip文件注释 对齐;
    check(_writeUInt16(self,zipCommentLen));//Zip文件注释长度;
    if (zipCommentLen>0)
        check(_writeAlignSkip(self,zipCommentLen));
    assert_align(self);
    return  _writeFlush(self);  
}
