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

inline static uint16_t readUInt16(const TByte* buf){
    return buf[0]|(buf[1]<<8);
}
inline static uint32_t readUInt32(const TByte* buf){
    return buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);
}

#define kBufSize                    (4*1024)

#define kENDHEADERMAGIC             (0x06054b50)
#define kMaxEndGlobalComment        (1<<(2*8)) //2 byte
#define kMinEndCentralDirectory     (22)  //struct size
#define kCENTRALHEADERMAGIC         (0x02014b50)
#define kMinFileHeader              (46)  //struct size

//反向找到第一个kENDHEADERMAGIC的位置;
static bool _UnZipper_searchEndCentralDirectory(UnZipper* self,ZipFilePos_t* out_endCDir_pos){
    TByte*       buf=self->_buf;
    ZipFilePos_t max_back_pos = kMaxEndGlobalComment+kMinEndCentralDirectory;
    if (max_back_pos>self->_fileLength)
        max_back_pos=self->_fileLength;
    ZipFilePos_t readed_pos=0;
    uint32_t     cur_value=0;
    while (readed_pos<max_back_pos) {
        ZipFilePos_t readLen=max_back_pos-readed_pos;
        if (readLen>kBufSize) readLen=kBufSize;
        readed_pos+=readLen;
        if (!fileSeek64(self->_file,self->_fileLength-readed_pos,SEEK_SET)) return false;
        if (!fileRead(self->_file,buf,buf+readLen)) return false;
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

//获得EndCentralDirectory的有用信息;
static bool _UnZipper_ReadEndCentralDirectory(UnZipper* self){
    TByte*  buf=self->_buf;
    assert(kBufSize>=kMinEndCentralDirectory);
    if (!fileSeek64(self->_file,self->_endCDirInfo.endCDir_pos,SEEK_SET)) return false;
    if (!fileRead(self->_file,buf,buf+kMinEndCentralDirectory)) return false;
    self->_endCDirInfo.fileHeader_count=readUInt16(buf+8);
    self->_endCDirInfo.fileHeader_size=readUInt32(buf+12);
    self->_endCDirInfo.fileHeader_pos=readUInt32(buf+16);
    return true;
}

//缓存所有FileHeader数据和其在缓存中的位置;
static bool _UnZipper_cacheFileHeader(UnZipper* self){
    assert(self->_cache_fileHeader==0);
    const int fileCount=UnZipper_FileEntry_count(self);
    TByte* buf=(TByte*)malloc(self->_endCDirInfo.fileHeader_size
                              +sizeof(int32_t)*(fileCount+1));
    self->_cache_fileHeader=buf;
    if (!fileSeek64(self->_file,self->_endCDirInfo.fileHeader_pos,SEEK_SET)) return false;
    if (!fileRead(self->_file,buf,buf+self->_endCDirInfo.fileHeader_size)) return false;
    
    size_t alignBuf=_hpatch_align_upper((buf+self->_endCDirInfo.fileHeader_size),sizeof(int));
    self->_fileHeaderOffsets=(int*)alignBuf;
    int curOffset=0;
    for (int i=0; i<fileCount; ++i) {
        if (kCENTRALHEADERMAGIC!=readUInt32(buf+curOffset)) return false;
        self->_fileHeaderOffsets[i]=curOffset;
        int fileNameLen=readUInt16(buf+curOffset+28);
        int extraFieldLen=readUInt16(buf+curOffset+30);
        int fileCommentLen=readUInt16(buf+curOffset+32);
        curOffset+= kMinFileHeader + fileNameLen+extraFieldLen+fileCommentLen;
        if (curOffset>self->_endCDirInfo.fileHeader_size) return false;
    }
    return true;
}


void UnZipper_init(UnZipper* self){
    memset(self,0,sizeof(*self));
}
bool UnZipper_close(UnZipper* self){
    self->_fileLength=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    if (self->_cache_fileHeader) { free(self->_cache_fileHeader); self->_cache_fileHeader=0; }
    return fileClose(&self->_file);
}

bool UnZipper_openRead(UnZipper* self,const char* zipFileName){
    hpatch_StreamPos_t fileLength=0;
    assert(self->_file==0);
    if (!fileOpenForRead(zipFileName,&self->_file,&fileLength)) return false;
    self->_fileLength=(ZipFilePos_t)fileLength;
    if (self->_fileLength!=fileLength) return false;
    assert(self->_buf==0);
    self->_buf=(unsigned char*)malloc(kBufSize);
    
    if (!_UnZipper_searchEndCentralDirectory(self,&self->_endCDirInfo.endCDir_pos)) return false;
    if (!_UnZipper_ReadEndCentralDirectory(self)) return false;
    if (!_UnZipper_cacheFileHeader(self)) return false;
    return true;
}


    inline static const TByte* fileHeaderBuf(const UnZipper* self,int fileIndex){
        return self->_cache_fileHeader+self->_fileHeaderOffsets[fileIndex];
    }
bool  UnZipper_FileEntry_isCompressed(const UnZipper* self,int fileIndex){
    uint16_t compressType=readUInt16(fileHeaderBuf(self,fileIndex)+10);
    return compressType!=0;
}
ZipFilePos_t UnZipper_FileEntry_compressedSize(const UnZipper* self,int fileIndex){
    return readUInt32(fileHeaderBuf(self,fileIndex)+20);
}
ZipFilePos_t UnZipper_FileEntry_uncompressedSize(const UnZipper* self,int fileIndex){
    return readUInt32(fileHeaderBuf(self,fileIndex)+24);
}

bool UnZipper_FileEntry_decompress(UnZipper* self,int fileIndex,unsigned char* dst){
    return false;
}

//----

void Zipper_init(Zipper* self){
    memset(self,0,sizeof(*self));
}
bool Zipper_close(Zipper* self){
    return fileClose(&self->_file);
}

bool Zipper_openWrite(Zipper* self,const char* zipFileName,int fileEntryMaxCount){
    assert(self->_file==0);
    if (!fileOpenForCreateOrReWrite(zipFileName,&self->_file)) return false;
    self->_fileEntryMaxCount=fileEntryMaxCount;
    return true;
}

bool Zipper_FileEntry_append(Zipper* self,UnZipper* srcZip,int srcFileIndex){
    return  false;
}

bool Zipper_FileEntry_appendWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                 const unsigned char* data,size_t dataSize,bool isNeedCompress){
    return  false;
}

bool Zipper_FileHeader_append(Zipper* self,UnZipper* srcZip,int srcFileIndex){
        return  false;
}

bool Zipper_EndCentralDirectory_append(Zipper* self,UnZipper* srcZip){
        return  false;
}
