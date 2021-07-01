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
#include "Zipper.h"
#include <string.h>
#include <ctype.h> //for isspace
#include "../../HDiffPatch/file_for_patch.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff_types.h"
#include "patch_types.h"
#include "../../HDiffPatch/libParallel/parallel_channel.h"
#include "../../HDiffPatch/compress_plugin_demo.h"
#include "../../HDiffPatch/decompress_plugin_demo.h"

static const TCompressPlugin_zlib zipCompatibleCompressPlugin={
    {_zlib_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_zlib_compress},
            9,MAX_MEM_LEVEL,-MAX_WBITS,hpatch_FALSE};
static const hdiff_TCompress*   compressPlugin  =&zipCompatibleCompressPlugin.base;
static hpatch_TDecompress*      decompressPlugin=&zlibDecompressPlugin;

#if (_IS_NEED_FIXED_ZLIB_VERSION)
#   define  kNormalizedZlibVersion         "1.2.11" //fixed zlib version
#endif

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
        check(self->stream->read(self->stream,fileLength-readed_pos,buf,buf+readLen));
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
    check(self->stream->read(self->stream,endCentralDirectory_pos+8,buf,buf+readLen));
    *out_fileCount=readUInt16(buf+(8-8));
    *out_centralDirectory_pos=readUInt32(buf+(16-8));
    return true;
}

static bool _searchID_in_block(const hpatch_TStreamInput* stream,ZipFilePos_t nodePos,ZipFilePos_t sumNodeSize,
                               hpatch_uint32_t searchID,ZipFilePos_t* out_IDNodePos,ZipFilePos_t* out_IDNodeSize){
    *out_IDNodePos=0;
    *out_IDNodeSize=0;
    TByte buf[8];
    while (sumNodeSize>0) {
        check(8+4<=sumNodeSize); //sizeof(size+ID)
        check(stream->read(stream,nodePos,buf,buf+8));
        hpatch_StreamPos_t nodeSize=readUInt64(buf);
        check(4<=nodeSize);
        check(8+nodeSize<=sumNodeSize);
        check(stream->read(stream,nodePos+8,buf,buf+4));
        hpatch_uint32_t ID=readUInt32(buf);
        if (ID==searchID){
            *out_IDNodePos=nodePos+8; //pos to ID
            *out_IDNodeSize=(ZipFilePos_t)nodeSize;
            return true;
        }
        //next node
        nodePos+=8+(ZipFilePos_t)nodeSize;
        sumNodeSize-=8+(ZipFilePos_t)nodeSize;
    }
    return true;
}

bool UnZipper_searchApkV2Sign(const hpatch_TStreamInput* stream,hpatch_StreamPos_t centralDirectory_pos,
                              ZipFilePos_t* v2sign_topPos,ZipFilePos_t* out_blockSize,bool* out_isHaveV3Sign){
    *v2sign_topPos=(ZipFilePos_t)centralDirectory_pos; //default not found
    if (out_blockSize) *out_blockSize=0;
    //nodePos_begin=v2sign_topPos-8;
    //sumNodeSize=blockSize-8-APKSigningTagLen;
    
    //tag
    const size_t APKSigningTagLen=16;
    const char* APKSigningTag="APK Sig Block 42";
    if (APKSigningTagLen>centralDirectory_pos) return true; //not found
    ZipFilePos_t APKSigningBlockTagPos=(ZipFilePos_t)centralDirectory_pos-APKSigningTagLen;
    TByte buf[APKSigningTagLen];
    check(stream->read(stream,APKSigningBlockTagPos,buf,buf+APKSigningTagLen));
    if (0!=memcmp(buf,APKSigningTag,APKSigningTagLen)) return true;//not found
    //bottom size
    check(8<=APKSigningBlockTagPos);
    ZipFilePos_t blockSizeBottomPos=APKSigningBlockTagPos-8;
    check(stream->read(stream,blockSizeBottomPos,buf,buf+8));
    hpatch_StreamPos_t blockSize=readUInt64(buf);
    //top
    check(blockSize+8<=centralDirectory_pos);
    ZipFilePos_t blockTopSizePos=(ZipFilePos_t)centralDirectory_pos-(ZipFilePos_t)blockSize-8;
    check(stream->read(stream,blockTopSizePos,buf,buf+8));
    check(blockSize==readUInt64(buf)); //check top size
    
    *v2sign_topPos=blockTopSizePos;
    if (out_blockSize) *out_blockSize=(ZipFilePos_t)blockSize;
    if (out_isHaveV3Sign){
        const hpatch_uint32_t kV3SignID=0xf05368c0;
        ZipFilePos_t IDNodePos;
        ZipFilePos_t IDNodeSize;
        if (!_searchID_in_block(stream,blockTopSizePos+8,(ZipFilePos_t)blockSize-8-APKSigningTagLen,
                                kV3SignID,&IDNodePos,&IDNodeSize)) return false; //error
        *out_isHaveV3Sign=(IDNodeSize>0);
    }
    return true;//found
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

bool UnZipper_file_is_sameName(const UnZipper* self,int fileIndex,const char* fileName,int fileNameLen){
    return (UnZipper_file_nameLen(self,fileIndex)==fileNameLen)&&
           (0==memcmp(UnZipper_file_nameBegin(self,fileIndex),fileName,fileNameLen));
}

int UnZipper_searchFileIndexByName(const UnZipper* self,const char* fileName,int fileNameLen){
    int fCount=UnZipper_fileCount(self);
    for (int i=0; i<fCount; ++i) {
        if (UnZipper_file_is_sameName(self,i,fileName,fileNameLen))
            return i;
    }
    return -1;
}

    static bool _find_ApkV2orV3SignTag_in_c_string(const char* data){
        const char* kApkTag="X-Android-APK-Signed";
        const char* pos=strstr(data,kApkTag);
        if (pos==0) return false;
        pos+=strlen(kApkTag);
        const char* posEnd=strchr(pos,'\n');
        if (posEnd==0) posEnd=data+strlen(data);
        const char* posBegin=strchr(pos,':');
        if ((posBegin==0)||(posBegin>=posEnd)) return false;
        ++posBegin;
        while ((posBegin<posEnd)&&isspace(*posBegin)) ++posBegin;
        while ((posBegin<posEnd)&&isspace(posEnd[-1])) --posEnd;
        if (posBegin>=posEnd) return false;
        return '2'==(*posBegin);
    }

    static bool _UnZipper_file_isHaveApkV2orV3SignTag_in_ApkV1SignFile(UnZipper* self,int fileIndex){
        if (!UnZipper_file_isApkV1_or_jarSign(self,fileIndex)) return false;
        
        //match fileType
        const char* kJarSignSF=".SF";
        const size_t kJarSignSFLen=strlen(kJarSignSF);
        int fnameLen=UnZipper_file_nameLen(self,fileIndex);
        if (fnameLen<(int)kJarSignSFLen) return false;
        const char* fnameBegin=UnZipper_file_nameBegin(self,fileIndex);
        if (0!=memcmp(fnameBegin+fnameLen-kJarSignSFLen,kJarSignSF,kJarSignSFLen)) return false;
        //match V2 tag in txt file
        size_t fsize=UnZipper_file_uncompressedSize(self,fileIndex);
        TByte* data=(TByte*)malloc(fsize+1); //+1 for '\0'
        check(data!=0);
        hpatch_TStreamOutput stream;
        mem_as_hStreamOutput(&stream,data,data+fsize);
        if(!UnZipper_fileData_decompressTo(self,fileIndex,&stream,0)) {
            free(data); return false; }
        data[fsize]='\0';
        bool result=_find_ApkV2orV3SignTag_in_c_string((char*)data);
        free(data);
        return result;
    }

bool UnZipper_isHaveApkV2orV3SignTag_in_ApkV1SignFile(UnZipper* self){
    int fCount=UnZipper_fileCount(self);
    for (int i=fCount-1; i>=0; --i) {
        if (_UnZipper_file_isHaveApkV2orV3SignTag_in_ApkV1SignFile(self,i))
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

bool UnZipper_file_isReCompressedByApkV2Sign(const UnZipper* self,int fileIndex){
    return UnZipper_isHaveApkV2Sign(self)
                && UnZipper_file_isCompressed(self,fileIndex)
                && UnZipper_file_isApkV1_or_jarSign(self,fileIndex);
}

bool UnZipper_isHaveApkV3Sign(const UnZipper* self){
    if (!UnZipper_isHaveApkV2Sign(self)) return false;
    return self->_isHaveV3Sign;
}

//缓存相关信息并规范化数据;
static bool _UnZipper_vce_normalized(UnZipper* self,bool isFileDataOffsetMatch,ZipFilePos_t* out_fileDataEndPos){
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
    *out_fileDataEndPos=0;
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
        ZipFilePos_t curFileDataEndPos=self->_fileDataOffsets[i]+self->_fileCompressedSizes[i];
        if (curFileDataEndPos>*out_fileDataEndPos)
            *out_fileDataEndPos=curFileDataEndPos;
        
        uint16_t fileTag=readUInt16(headBuf+8);//标志;
        if ((fileTag&(1<<3))!=0){//have descriptor?
            writeUInt16_to(headBuf+8,fileTag&(~(1<<3)));//normalized 标志中去掉data descriptor标识;
            const uint32_t crc=UnZipper_file_crc32(self,i);
            const uint32_t uncompressedSize=UnZipper_file_uncompressedSize(self,i);
            TByte buf[16];
            check(UnZipper_fileData_read(self,self->_fileDataOffsets[i]+self->_fileCompressedSizes[i],buf,buf+16));
            if ((readUInt32(buf)==crc)&&(readUInt32(buf+8)==uncompressedSize))
                self->_dataDescriptors[i]=kDataDescriptor_12;
            else if ((readUInt32(buf+4)==crc)&&(readUInt32(buf+12)==uncompressedSize))
                self->_dataDescriptors[i]=kDataDescriptor_16;
            else {
                //check(false);
                printf("WARNING: zip format error, unknow data descriptor!\n");
                self->_dataDescriptors[i]=kDataDescriptor_12;
            }
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


static inline void _UnZipper_close_fvce(UnZipper* self){
    if (self->_cache_fvce) { free(self->_cache_fvce); self->_cache_fvce=0; self->_fvce_size=0; }
}

bool UnZipper_close(UnZipper* self){
    self->stream=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    _UnZipper_close_fvce(self);
    if (self->_fileStream.m_file!=0) { check(hpatch_TFileStreamInput_close(&self->_fileStream)); }
    return true;
}

static bool _UnZipper_open_begin(UnZipper* self){
    assert(self->_buf==0);
    self->_buf=(unsigned char*)malloc(kBufSize);
    check(self->_buf!=0);
    self->_isDataNormalized=false;
    return true;
}

static bool _UnZipper_open_fvce(UnZipper* self,ZipFilePos_t fvceSize,int fileCount){
    assert(self->_cache_fvce==0);
    self->_fvce_size=fvceSize;
    self->_cache_fvce=(TByte*)malloc(self->_fvce_size+1*fileCount
                                    +sizeof(ZipFilePos_t)*(fileCount+1)+sizeof(uint32_t)*fileCount*2);
    check(self->_cache_fvce!=0);
    self->_dataDescriptors=self->_cache_fvce+self->_fvce_size;
    size_t alignBuf=_hpatch_align_upper((self->_dataDescriptors+fileCount),sizeof(ZipFilePos_t));
    self->_fileDataOffsets=(ZipFilePos_t*)alignBuf;
    self->_fileHeaderOffsets=(uint32_t*)(self->_fileDataOffsets+fileCount);
    self->_fileCompressedSizes=(uint32_t*)(self->_fileHeaderOffsets+fileCount);
    return true;
}

bool UnZipper_openStream(UnZipper* self,const hpatch_TStreamInput* zipFileStream,
                         bool isDataNormalized,bool isFileDataOffsetMatch){
    check(_UnZipper_open_begin(self));
    self->stream=zipFileStream;
    //_sreachVCE
    ZipFilePos_t endCentralDirectory_pos=0;
    ZipFilePos_t centralDirectory_pos=0;
    uint32_t     fileCount=0;
    ZipFilePos_t v2sign_topPos=0;
    check(_UnZipper_searchEndCentralDirectory(self,&endCentralDirectory_pos));
    check(_UnZipper_searchCentralDirectory(self,endCentralDirectory_pos,&centralDirectory_pos,&fileCount));
    check(UnZipper_searchApkV2Sign(self->stream,centralDirectory_pos,&v2sign_topPos,0,&self->_isHaveV3Sign));

    // now don't know fileDataEndPos value
    const ZipFilePos_t kNullSize=1024*4;
    ZipFilePos_t ripe_fileDataEndPos=(v2sign_topPos>kNullSize)?(v2sign_topPos-kNullSize):0;
    assert(self->_cache_fvce==0);
    for (int i=0; i<2; ++i){ //try 2 times
        check(_UnZipper_open_fvce(self,(ZipFilePos_t)self->stream->streamSize-ripe_fileDataEndPos,fileCount));
        self->_vce=self->_cache_fvce+(v2sign_topPos-ripe_fileDataEndPos);
        self->_centralDirectory=self->_vce+(centralDirectory_pos-v2sign_topPos);
        self->_endCentralDirectory=self->_vce+(endCentralDirectory_pos-v2sign_topPos);
        
        check(UnZipper_fileData_read(self,ripe_fileDataEndPos,self->_cache_fvce,self->_cache_fvce+self->_fvce_size));
        ZipFilePos_t fileDataEndPos;
        check(_UnZipper_vce_normalized(self,isFileDataOffsetMatch,&fileDataEndPos));
        if (fileDataEndPos>=ripe_fileDataEndPos){ //ok
            ZipFilePos_t nullSize=(fileDataEndPos-ripe_fileDataEndPos);
            if (nullSize>0){//need move data
                self->_vce-=nullSize;
                self->_centralDirectory-=nullSize;
                self->_endCentralDirectory-=nullSize;
                self->_fvce_size-=nullSize;
                memmove(self->_cache_fvce,self->_cache_fvce+nullSize,self->_fvce_size);
            }
            break;
        }else{ //continue
            _UnZipper_close_fvce(self);
            ripe_fileDataEndPos=fileDataEndPos;
        }
    }
    check(self->_cache_fvce!=0);
    self->_isDataNormalized=isDataNormalized;
    self->_isFileDataOffsetMatch=isFileDataOffsetMatch;
    return true;
}

bool UnZipper_openFile(UnZipper* self,const char* zipFileName,bool isDataNormalized,bool isFileDataOffsetMatch){
    check(hpatch_TFileStreamInput_open(&self->_fileStream,zipFileName));
    return UnZipper_openStream(self,&self->_fileStream.base,isDataNormalized,isFileDataOffsetMatch);
}

bool UnZipper_openVirtualVCE(UnZipper* self,ZipFilePos_t fvce_size,int fileCount){
    check(_UnZipper_open_begin(self));
    check(_UnZipper_open_fvce(self,fvce_size,fileCount));
    mem_as_hStreamInput(&self->_fileStream.base,self->_cache_fvce,self->_cache_fvce+self->_fvce_size);
    self->stream=&self->_fileStream.base;
    return true;
}

bool UnZipper_updateVirtualVCE(UnZipper* self,bool isDataNormalized,size_t zipCESize){
    ZipFilePos_t endCentralDirectory_pos=0;
    ZipFilePos_t centralDirectory_pos=(ZipFilePos_t)(self->stream->streamSize-zipCESize);
    ZipFilePos_t v2sign_topPos=0;
    check(_UnZipper_searchEndCentralDirectory(self,&endCentralDirectory_pos));
    check(UnZipper_searchApkV2Sign(self->stream,centralDirectory_pos,&v2sign_topPos,0,&self->_isHaveV3Sign));
    ZipFilePos_t fileDataEndPos=0;
    self->_vce=self->_cache_fvce+(v2sign_topPos-fileDataEndPos);
    self->_centralDirectory=self->_vce+(centralDirectory_pos-v2sign_topPos);
    self->_endCentralDirectory=self->_vce+(endCentralDirectory_pos-v2sign_topPos);
    self->_isDataNormalized=isDataNormalized;
    
    bool isFileDataOffsetMatch=true;
    check(_UnZipper_vce_normalized(self,isFileDataOffsetMatch,&fileDataEndPos));
    return true;
}

inline static const unsigned char* _at_file_compressType(const UnZipper* self,int fileIndex){
    return fileHeaderBuf(self,fileIndex)+10;
}
inline static uint16_t _file_compressType(const UnZipper* self,int fileIndex){
    return readUInt16(_at_file_compressType(self,fileIndex));
}
bool  UnZipper_file_isCompressed(const UnZipper* self,int fileIndex){
    uint16_t compressType=_file_compressType(self,fileIndex);
    return compressType!=0;
}
inline static const unsigned char* _at_file_uncompressedSize(const UnZipper* self,int fileIndex){
    return fileHeaderBuf(self,fileIndex)+24;
}
ZipFilePos_t UnZipper_file_uncompressedSize(const UnZipper* self,int fileIndex){
    return readUInt32(_at_file_uncompressedSize(self,fileIndex));
}
ZipFilePos_t UnZipper_file_compressedSize(const UnZipper* self,int fileIndex){
    return self->_fileCompressedSizes[fileIndex];
}
inline static const unsigned char* _at_file_crc32(const UnZipper* self,int fileIndex){
    return fileHeaderBuf(self,fileIndex)+16;
}
uint32_t UnZipper_file_crc32(const UnZipper* self,int fileIndex){
    return readUInt32(_at_file_crc32(self,fileIndex));
}

#if (_IS_NEED_VIRTUAL_ZIP)
inline static void writeUInt(unsigned char* buf,uint32_t v,int size){
    for (int i=0;i<size;++i){
        buf[i]=(unsigned char)v;
        v>>=8;
    }
}
#endif

#if (_IS_NEED_VIRTUAL_ZIP)
void UnZipper_updateVirtualFileInfo(UnZipper* self,int fileIndex,ZipFilePos_t uncompressedSize,
                                    ZipFilePos_t compressedSize,uint32_t crc32){
    writeUInt((unsigned char*)_at_file_uncompressedSize(self,fileIndex),uncompressedSize,4);
    self->_fileCompressedSizes[fileIndex]=compressedSize;
    writeUInt((unsigned char*)_at_file_crc32(self,fileIndex),crc32,4);
}
#endif

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
    return hpatch_FALSE!=self->stream->read(self->stream,file_pos,buf,bufEnd);
}

bool UnZipper_fileData_copyTo(UnZipper* self,int fileIndex,
                              const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos){
    ZipFilePos_t fileSavedSize=UnZipper_file_compressedSize(self,fileIndex);
    ZipFilePos_t fileDataOffset=UnZipper_fileData_offset(self,fileIndex);
    return UnZipper_dataStream_copyTo(self,self->stream,fileDataOffset,
                                      fileDataOffset+fileSavedSize,outStream,writeToPos);
}

#define check_clear(v) { if (!(v)) { result=false; assert(false); goto clear; } }

bool UnZipper_compressedData_decompressTo(UnZipper* self,const hpatch_TStreamInput* codeStream,
                                          hpatch_StreamPos_t code_begin,hpatch_StreamPos_t code_end,
                                          hpatch_StreamPos_t uncompressedSize,
                                          const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos){
    bool result=true;
    _zlib_TDecompress* decHandle=_zlib_decompress_open_by(decompressPlugin,codeStream,code_begin,
                                                          code_end,0,self->_buf,(kBufSize>>1));
    TByte* dataBuf=self->_buf+(kBufSize>>1);
    hpatch_StreamPos_t  curWritePos=0;
    check_clear(decHandle!=0);
    while (curWritePos<uncompressedSize){
        hpatch_StreamPos_t readLen=(kBufSize>>1);
        if (readLen>(uncompressedSize-curWritePos)) readLen=uncompressedSize-curWritePos;
        check_clear(_zlib_decompress_part(decHandle,dataBuf,dataBuf+readLen));
        check_clear(outStream->write(outStream,writeToPos+curWritePos,dataBuf,dataBuf+readLen));
        curWritePos+=readLen;
    }
    if (!_zlib_is_decompress_finish(decompressPlugin,decHandle))
        printf("WARNING: zip format error, decompress not finish!\n");
clear:
    _zlib_decompress_close_by(decompressPlugin,decHandle);
    return result;
}

bool UnZipper_dataStream_copyTo(UnZipper* self,const hpatch_TStreamInput* dataStream,
                                hpatch_StreamPos_t data_begin,hpatch_StreamPos_t data_end,
                                const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos){
    TByte* buf=self->_buf;
    assert(data_begin<=data_end);
    assert(data_end<=dataStream->streamSize);
    while (data_begin<data_end) {
        ZipFilePos_t readLen=kBufSize;
        if (readLen>(data_end-data_begin)) readLen=(ZipFilePos_t)(data_end-data_begin);
        check(dataStream->read(dataStream,data_begin,buf,buf+readLen));
        check(outStream->write(outStream,writeToPos,buf,buf+readLen));
        writeToPos+=readLen;
        data_begin+=readLen;
    }
    return true;
}


bool UnZipper_fileData_decompressTo(UnZipper* self,int fileIndex,
                                    const hpatch_TStreamOutput* outStream,hpatch_StreamPos_t writeToPos){
    if (!UnZipper_file_isCompressed(self,fileIndex)){
        return UnZipper_fileData_copyTo(self,fileIndex,outStream,writeToPos);
    }
    
    uint16_t compressType=_file_compressType(self,fileIndex);
    check(Z_DEFLATED==compressType);

    ZipFilePos_t file_offset=UnZipper_fileData_offset(self,fileIndex);
    ZipFilePos_t file_compressedSize=UnZipper_file_compressedSize(self,fileIndex);
    ZipFilePos_t file_data_size=UnZipper_file_uncompressedSize(self,fileIndex);
    
    return UnZipper_compressedData_decompressTo(self,self->stream,file_offset,file_offset+file_compressedSize,
                                                file_data_size,outStream,writeToPos);
}

#if (_IS_USED_MULTITHREAD)
//----
struct TZipThreadWork {
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


namespace {

struct TMt_base {
    CChannel work_chan;
    CChannel data_chan;
    
    void on_error(){
        {
            CAutoLocker _auto_locker(_locker.locker);
            if (_is_on_error) return;
            _is_on_error=true;
        }
        closeAndClear();
    }
    
    bool start_threads(int threadCount,TThreadRunCallBackProc threadProc,void* workData,bool isUseThisThread){
        for (int i=0;i<threadCount;++i){
            if (isUseThisThread&&(i==threadCount-1)){
                thread_on(1);
                threadProc(i,workData);
            }else{
                thread_on(1);
                try{
                    thread_parallel(1,threadProc,workData,0,i);
                }catch(...){
                    thread_on(-1);
                    on_error();
                    return false;
                }
            }
        }
        return true;
    }
    inline void thread_end(){
        _end_chan.send((TChanData)1,true);
    }
    inline  explicit TMt_base(int workChanSize,int dataChanSize)
        :work_chan(workChanSize),data_chan(dataChanSize),_is_on_error(false),_is_thread_on(0) {}
    inline ~TMt_base() { closeAndClear(); wait_all_thread_end(); _end_chan.close(); while (_end_chan.accept(false)) {} }
    inline bool is_on_error()const{ CAutoLocker _auto_locker(_locker.locker); return _is_on_error; }
    
    inline void finish(){ // wait all threads exit
        close();
        wait_all_thread_end();
    }
    inline void wait_all_thread_end(){
        while(_is_thread_on){
            --_is_thread_on;
            _end_chan.accept(true);
        }
    }
protected:
    CHLocker  _locker;
    volatile bool _is_on_error;
    
    inline void close() {
        work_chan.close();
        data_chan.close();
    }
    void closeAndClear(){
        close();
        while(work_chan.accept(false)) {}
        while(data_chan.accept(false)) {}
    }
private:
    inline void thread_on(int threadNum){
        _is_thread_on+=threadNum;
    }
    volatile size_t _is_thread_on;
    CChannel  _end_chan;
};

struct _auto_thread_end_t{
    inline  explicit _auto_thread_end_t(TMt_base& mt) :_mt(mt) {  }
    inline ~_auto_thread_end_t() { _mt.thread_end(); }
    TMt_base&  _mt;
};

} //end namespace

struct TZipThreadWorks:public TMt_base{
//for thread
public:
    static void run(int threadIndex,void* workData){
        TZipThreadWorks* self=(TZipThreadWorks*)workData;
        _auto_thread_end_t _thread_end(*self);
        while (TZipThreadWork* work=self->getWork()){
            self->doWork(work);
            self->endWork(work);
        }
    }
private:
    inline TZipThreadWork* getWork(){//wait,return 0 exit thread
        return (TZipThreadWork*)work_chan.accept(true);
    }
    inline static void doWork(TZipThreadWork* work){
        size_t rsize=Zipper_compressData(work->inputData,work->inputDataSize,work->code,work->codeSize,
                                         work->compressLevel,work->compressMemLevel);
        if ((rsize==0)||(rsize!=work->codeSize))
            work->codeSize=0; //error or code size overflow
    }
    inline void endWork(TZipThreadWork* workResult){
        data_chan.send(workResult,true);
    }
public:
    explicit TZipThreadWorks(Zipper* zip,int workThreadNum)
        :TMt_base(1,workThreadNum),_zip(zip),_curWorkCount(0){ }
    inline void waitCanFastDispatchWork(){
        work_chan.is_can_fast_send(true);
    }
    inline void dispatchWork(TZipThreadWork* newWork){
        ++_curWorkCount;
        work_chan.send(newWork,true);
    }
    inline void finishDispatchWork(){
        work_chan.close();
    }
    TZipThreadWork* haveFinishWork(bool isWait){
        if (_curWorkCount==0) return 0;
        TZipThreadWork* result=(TZipThreadWork*)data_chan.accept(isWait);
        if (result) --_curWorkCount;
        return result;
    }
private:
    Zipper*             _zip;
    volatile int        _curWorkCount;
};
#else
    struct TZipThreadWork{};
    struct TZipThreadWorks{};
#endif //_IS_USED_MULTITHREAD


void Zipper_init(Zipper* self){
    memset(self,0,sizeof(*self));
}

bool Zipper_close(Zipper* self){
#if (_IS_USED_MULTITHREAD)
    if (self->_append_stream.threadWork){
        freeThreadWork(self->_append_stream.threadWork);
        self->_append_stream.threadWork=0;
    }
    if (self->_threadWorks){
        self->_threadNum=1;
        delete self->_threadWorks;
        self->_threadWorks=0;
    }
#endif
    
    self->_stream=0;
    self->_fileEntryCount=0;
    if (self->_buf) { free(self->_buf); self->_buf=0; }
    if (self->_fileStream.m_file) { check(hpatch_TFileStreamOutput_close(&self->_fileStream)); }
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
#if (_IS_NEED_FIXED_ZLIB_VERSION)
    check(0==strcmp(kNormalizedZlibVersion,zlibVersion()));//fixed zlib version
#endif
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
#if (_IS_USED_MULTITHREAD)
    threadNum=(threadNum<=self->_fileEntryMaxCount)?threadNum:self->_fileEntryMaxCount;
    self->_threadNum=(threadNum>=1)?threadNum:1;
    if (isUsedMT(self)){
        try {
            self->_threadWorks=new TZipThreadWorks(self,self->_threadNum-1);
            self->_threadWorks->start_threads(self->_threadNum-1,TZipThreadWorks::run,self->_threadWorks,false);
        } catch (...) {
            self->_threadNum=1; //close muti-thread
            if (self->_threadWorks){
                delete self->_threadWorks;
                self->_threadWorks=0;
            } 
        }
    }
#else
    self->_threadNum=1;
#endif
}

bool Zipper_openFile(Zipper* self,const char* zipFileName,int fileEntryMaxCount,
                      int ZipAlignSize,int compressLevel,int compressMemLevel){
    assert(self->_fileStream.m_file==0);
    check(hpatch_TFileStreamOutput_open(&self->_fileStream,zipFileName,(hpatch_StreamPos_t)(-1)));
    hpatch_TFileStreamOutput_setRandomOut(&self->_fileStream,hpatch_TRUE);
    return Zipper_openStream(self,&self->_fileStream.base,fileEntryMaxCount,
                             ZipAlignSize,compressLevel,compressMemLevel);
}

static bool _writeFlush(Zipper* self){
    size_t curBufLen=self->_curBufLen;
    if (curBufLen>0){
        self->_curBufLen=0;
        check(self->_stream->write(self->_stream,self->_curFilePos-curBufLen,self->_buf,self->_buf+curBufLen));
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
        check(self->_stream->write(self->_stream,self->_curFilePos-self->_curBufLen,data,data+len));
    }
    self->_curFilePos+=(ZipFilePos_t)len;
    return true;
}

#if (_IS_USED_MULTITHREAD)
inline static bool _writeSkip(Zipper* self,size_t skipLen){
    check(_writeFlush(self));
    self->_curFilePos+=(ZipFilePos_t)skipLen;
    return true;
}
#endif

inline static bool _writeBack(Zipper* self,size_t backPos,const TByte* data,size_t len){
    check(self->_stream->write(self->_stream,backPos,data,data+len));
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
    if (self->_isUpdateIsCompress){
        self->_isUpdateIsCompress=false;
        if (isCompressed!=self->_newIsCompress){
            isCompressed=self->_newIsCompress;
            if (!self->_newIsCompress)
                compressedSize=UnZipper_file_uncompressedSize(srcZip,srcFileIndex);
        }
    }
    uint16_t fileNameLen=UnZipper_file_nameLen(srcZip,srcFileIndex);
    uint16_t extraFieldLen=readUInt16(headBuf+30);
    uint16_t extraFieldLen_for_align=extraFieldLen;
    bool isNeedAlign=(!isFullInfo)&&(!isCompressed); //dir or 0 size file need align too, same AndroidSDK#zipalign
    if (isNeedAlign){
        size_t headInfoLen=30+fileNameLen+extraFieldLen;
        size_t skipLen=_getAlignSkipLen(self,self->_curFilePos+headInfoLen);
        if (skipLen>0){
            if (fileIndex==0){//只能扩展第一个entry的extraField来对齐;
                extraFieldLen_for_align+=(uint16_t)skipLen;
                //WARNING
                char fileName[hpatch_kPathMaxSize];
                if (fileNameLen+1>hpatch_kPathMaxSize) return false;
                memcpy(fileName,UnZipper_file_nameBegin(srcZip,srcFileIndex),fileNameLen);
                fileName[fileNameLen]='\0';
                printf("WARNING: \""); hpatch_printPath_utf8(fileName);
                printf("\" file's extraField adding %d byte 0 for align!\n",(int)skipLen);
            }else{
                check(_writeAlignSkip(self,skipLen));//利用entry之间的空间对齐;
            }
        }
    }
    if (!isFullInfo){
        self->_fileEntryOffsets[fileIndex]=self->_curFilePos;
    }
    check(_writeUInt32(self,isFullInfo?kCENTRALHEADERMAGIC:kLOCALHEADERMAGIC));
    if (isFullInfo)
        check(_write(self,headBuf+4,2));//创建时版本;
    check(_write(self,headBuf+6,4));//解开时需要的版本+标志;//其中 标志 中已经去掉了data descriptor标识;
    check(_writeUInt16(self,isCompressed?Z_DEFLATED:0));//压缩方法;
    check(_write(self,headBuf+12,4));//文件的最后修改时间+文件最后修改日期;
    if (self->_isUpdateCrc32){
        self->_isUpdateCrc32=false;
        check(_writeUInt32(self,self->_newCrc32));//更新CRC-32校验;
    }else{
        check(_write(self,headBuf+16,4));//CRC-32校验;
    }
    check(_writeUInt32(self,compressedSize));//压缩后大小;
    check(_write(self,headBuf+24,30-24));//压缩前大小--文件名称长度;
    check(_writeUInt16(self,extraFieldLen_for_align));//扩展字段长度;
    
    uint16_t fileCommentLen=0;//文件注释长度;
    if (isFullInfo){
        fileCommentLen=readUInt16(headBuf+32);
        check(_writeUInt16(self,fileCommentLen));
        check(_write(self,headBuf+34,42-34));//文件开始的分卷号--文件外部属性;
        check(_writeUInt32(self,self->_fileEntryOffsets[fileIndex]));//对应文件在文件中的偏移;
    }
    check(_write(self,headBuf+46,fileNameLen+extraFieldLen));//文件名称 + 扩展字段;
    if (extraFieldLen_for_align>extraFieldLen) //对齐;
        check(_writeAlignSkip(self,extraFieldLen_for_align-extraFieldLen));
    if (fileCommentLen>0)
        check(_write(self,headBuf+46+fileNameLen+extraFieldLen,fileCommentLen));//[+文件注释];
    if (isNeedAlign) assert_align(self);//对齐检查;
    return  true;
}


size_t Zipper_compressData_maxCodeSize(size_t dataSize){
    return (size_t)compressPlugin->maxCompressedSize(dataSize);
}

size_t Zipper_compressData(const unsigned char* data,size_t dataSize,unsigned char* out_code,
                           size_t codeSize,int kCompressLevel,int kCompressMemLevel){
    const int kCodeBufSize=1024;
    TByte codeBuf[kCodeBufSize];
    hdiff_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,out_code,out_code+codeSize);
    _zlib_TCompress* compressHandle=_zlib_compress_open_by(compressPlugin,kCompressLevel,kCompressMemLevel,
                                                           &stream,codeBuf,kCodeBufSize);
    if (compressHandle==0) return 0;//error;
    int outStream_isCanceled=0;//false
    hpatch_StreamPos_t curWritedPos=0;
    int is_data_end=true;
    if (!_zlib_compress_part(compressHandle,data,data+dataSize,
                             is_data_end,&curWritedPos,&outStream_isCanceled)) return 0;//error
    if (!_zlib_compress_close_by(compressPlugin,compressHandle)) return 0;//error
    return (size_t)curWritedPos;
}

hpatch_BOOL Zipper_file_append_stream::_append_part_input(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t pos,
                                                          const unsigned char *part_data,
                                                          const unsigned char *part_data_end){
    Zipper_file_append_stream* append_state=(Zipper_file_append_stream*)stream->streamImport;
    assert(append_state->inputPos==pos);
    size_t partSize=(size_t)(part_data_end-part_data);
    append_state->inputPos+=partSize;
    if (append_state->inputPos>append_state->streamSize) return hpatch_FALSE;//error
    int is_data_end=(append_state->inputPos==append_state->streamSize);
#if (_IS_USED_MULTITHREAD)
    if (append_state->threadWork){
        memcpy(append_state->threadWork->inputData+append_state->inputPos-partSize,part_data,partSize);
        return hpatch_TRUE;
    } else
#endif
    if (append_state->compressHandle){
        int outStream_isCanceled=0;//false
        hpatch_StreamPos_t curWritedPos=append_state->outputPos;
        if(!_zlib_compress_part(append_state->compressHandle,part_data,part_data_end,
                                is_data_end,&curWritedPos,&outStream_isCanceled)) return hpatch_FALSE;//error
        assert(curWritedPos==append_state->outputPos);
        return hpatch_TRUE;
    }else{
        return _append_part_output(stream,append_state->outputPos,part_data,part_data_end);
    }
}

hpatch_BOOL Zipper_file_append_stream::_append_part_output(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t pos,
                                                           const unsigned char *part_data,
                                                           const unsigned char *part_data_end){
    Zipper_file_append_stream* append_state=(Zipper_file_append_stream*)stream->streamImport;
    assert(append_state->outputPos==pos);
    size_t partSize=(size_t)(part_data_end-part_data);
    append_state->outputPos+=partSize;
    return _write(append_state->self,part_data,partSize);
}


inline static bool _zipper_curFile_isCompressed(const Zipper* self,UnZipper* srcZip,int srcFileIndex){
    return self->_isUpdateIsCompress ? self->_newIsCompress :
                            UnZipper_file_isCompressed(srcZip,srcFileIndex);
}

bool Zipper_file_append_begin(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                              bool appendDataIsCompressed,size_t dataUncompressedSize,size_t dataCompressedSize){
    return Zipper_file_append_beginWith(self,srcZip,srcFileIndex,appendDataIsCompressed,dataUncompressedSize,
                                        dataCompressedSize,self->_compressLevel,self->_compressMemLevel);
}
bool Zipper_file_append_beginWith(Zipper* self,UnZipper* srcZip,int srcFileIndex,
                                  bool appendDataIsCompressed,size_t dataUncompressedSize,size_t dataCompressedSize,
                                  int curFileCompressLevel,int curFileCompressMemLevel){
    const bool dstFileIsCompressed=_zipper_curFile_isCompressed(self,srcZip,srcFileIndex);
    if (dstFileIsCompressed&&(!appendDataIsCompressed))
        checkCompressSet(curFileCompressLevel,curFileCompressMemLevel);
    if ((!dstFileIsCompressed)&&(appendDataIsCompressed)){
        assert(false);//now need input decompressed data;
        return false; // for example: UnZipper_fileData_decompressTo(Zipper_file_append_part_as_stream());
    }
    if (0==dataCompressedSize){
        check((!appendDataIsCompressed)||(dataUncompressedSize==0));
        if (dataUncompressedSize!=0)
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
    self->_fileCompressedSizes[curFileIndex]=dstFileIsCompressed?(uint32_t)dataCompressedSize: //maybe temp value
                                            (uint32_t)dataUncompressedSize; //finally value
    check(_write_fileHeaderInfo(self,curFileIndex,srcZip,srcFileIndex,false));//out file info
    
    append_state->self=self;
    append_state->inputPos=0;
    append_state->outputPos=0;
    append_state->curFileIndex=curFileIndex;
    append_state->streamImport=append_state;
    append_state->streamSize=appendDataIsCompressed?dataCompressedSize:dataUncompressedSize; //finally value
    append_state->write=Zipper_file_append_stream::_append_part_input;
    assert(append_state->compressHandle==0);
    assert(append_state->threadWork==0);
    if (dstFileIsCompressed&&(!appendDataIsCompressed)){//compress data
#if (_IS_USED_MULTITHREAD)
        if (isUsedMT(self)&&(dataCompressedSize>0)){
            self->_threadWorks->waitCanFastDispatchWork();
            append_state->threadWork=newThreadWork(dataUncompressedSize,dataCompressedSize,self->_curFilePos,
                                                   curFileCompressLevel,curFileCompressMemLevel);
            if ((append_state->threadWork)&&(!_writeSkip(self,dataCompressedSize)))
                return false;
            //else continue : this thread do compress
        }
#endif
        if (!append_state->threadWork){
            append_state->compressOutStream.streamImport=append_state;
            append_state->compressOutStream.streamSize=self->_fileCompressedSizes[curFileIndex];//not used
            append_state->compressOutStream.write=Zipper_file_append_stream::_append_part_output;
            append_state->compressHandle=_zlib_compress_open_by(compressPlugin,curFileCompressLevel,curFileCompressMemLevel,
                                                                &append_state->compressOutStream,self->_codeBuf,kBufSize);
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

#if (_IS_USED_MULTITHREAD)
static bool _dispose_filishedThreadWork(Zipper* self,bool isWait){
    while (true) {
        TZipThreadWork* work=self->_threadWorks->haveFinishWork(isWait);
        if (work){
            bool ret=(work->codeSize>0);
            if (ret) check(_writeBack(self,work->writePos,work->code,work->codeSize));
            freeThreadWork(work);
            if (!ret) 
                return false;
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
        if ((append_state->inputPos==0)&&(append_state->outputPos==0)){
            Byte emptyBuf=0; //compress empty file
            check(append_state->write(append_state,0,&emptyBuf,&emptyBuf));
        }
        check_clear(_zlib_compress_close_by(compressPlugin,append_state->compressHandle));
    }
    
    check_clear(append_state->inputPos==append_state->streamSize);
#if (_IS_USED_MULTITHREAD)
    if (isUsedMT(self)) {
        if (append_state->threadWork){
            self->_threadWorks->dispatchWork(append_state->threadWork);
            append_state->threadWork=0;
        }
        check_clear(_dispose_filishedThreadWork(self,false));
    }
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
    return hpatch_FALSE!=append_state->write(append_state,append_state->inputPos,part_data,part_data+partSize);
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
    if (srcZip->_cache_fvce == srcZip->_centralDirectory)
        return true;
    return _write(self,srcZip->_cache_fvce,srcZip->_centralDirectory-srcZip->_cache_fvce);
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
    check(!self->_isUpdateCrc32);
    check(!self->_isUpdateIsCompress);
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
#if (_IS_USED_MULTITHREAD)
    if (isUsedMT(self)){
        self->_threadWorks->finishDispatchWork();
        check(_dispose_filishedThreadWork(self,true));
    }
#endif
    return true;
}



bool Zipper_file_append_set_new_crc32(Zipper* self,uint32_t newCrc32){
    check(!self->_isUpdateCrc32);
    self->_isUpdateCrc32=true;
    self->_newCrc32=newCrc32;
    return true;
}

bool Zipper_fileHeader_append_set_new_crc32(Zipper* self,uint32_t newCrc32){
    return Zipper_file_append_set_new_crc32(self,newCrc32);
}


bool Zipper_file_append_set_new_isCompress(Zipper* self,bool newIsCompress){
    check(!self->_isUpdateIsCompress);
    self->_isUpdateIsCompress=true;
    self->_newIsCompress=newIsCompress;
    return true;
}

bool Zipper_fileHeader_append_set_new_isCompress(Zipper* self,bool newIsCompress){
    return Zipper_file_append_set_new_isCompress(self,newIsCompress);
}
