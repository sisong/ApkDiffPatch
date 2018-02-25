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

//反向找到第一个kENDHEADERMAGIC的位置;
bool _UnZipper_searchEndCentralDirectory(UnZipper* self,ZipFilePos_t* out_endCDir_pos){
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
                return true;
            }
        }
    }
    return false;
}

//获得EndCentralDirectory的有用信息;
bool _UnZipper_ReadEndCentralDirectory(UnZipper* self){
    TByte*       buf=self->_buf;
    assert(kBufSize>=kMinEndCentralDirectory);
    if (!fileSeek64(self->_file,self->_endCDirInfo.endCDir_pos,SEEK_SET)) return false;
    if (!fileRead(self->_file,buf,buf+kMinEndCentralDirectory)) return false;
    self->_endCDirInfo.fileHeader_count=readUInt16(buf+10);
    if (self->_endCDirInfo.fileHeader_count!=readUInt16(buf+8)) return false; //todo: 支持分卷;
    self->_endCDirInfo.fileHeader_size=readUInt32(buf+12);
    self->_endCDirInfo.fileHeader_pos=readUInt32(buf+16);
    return false;
}


void UnZipper_init(UnZipper* self){
    memset(self,0,sizeof(*self));
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
    return true;
}

bool UnZipper_close(UnZipper* self){
    self->_fileLength=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    return fileClose(&self->_file);
}

//----

void Zipper_init(Zipper* self){
    memset(self,0,sizeof(*self));
}

bool Zipper_openWrite(Zipper* self,const char* zipFileName){
    assert(self->_file==0);
    if (!fileOpenForCreateOrReWrite(zipFileName,&self->_file)) return false;
    return true;
}

bool Zipper_close(Zipper* self){
    return fileClose(&self->_file);
}

