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
//todo: support zip64

typedef unsigned long ZipFilePos_t;

struct _t_EndCentralDirectoryInfo{
    ZipFilePos_t  endCDir_pos;
    unsigned int  fileHeader_count;
    ZipFilePos_t  fileHeader_size;
    ZipFilePos_t  fileHeader_pos;
};

struct UnZipper{
    unsigned char*  _buf;
    FILE*           _file;
    ZipFilePos_t    _fileLength;
    _t_EndCentralDirectoryInfo _endCDirInfo;
};
void UnZipper_init(UnZipper* self);
bool UnZipper_openRead(UnZipper* self,const char* zipFileName);
bool UnZipper_close(UnZipper* self);

struct Zipper{
    FILE*           _file;
};
void Zipper_init(Zipper* self);
bool Zipper_openWrite(Zipper* self,const char* zipFileName);
bool Zipper_close(Zipper* self);

#endif /* Zipper_h */
