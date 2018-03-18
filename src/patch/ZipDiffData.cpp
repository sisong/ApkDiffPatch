//  ZipDiffData.cpp
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
#include "ZipDiffData.h"
#include <string.h>
#include "patch_types.h"

void ZipDiffData_init(ZipDiffData* self){
    memset(self,0,sizeof(ZipDiffData));
}
void ZipDiffData_close(ZipDiffData* self){
    if (self->_buf) { free(self->_buf); self->_buf=0; }
}

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }
#define  check_clear(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; assert(false); if (!_isInClear){ goto clear; } } }

#define checkUnpackSize(src_code,src_code_end,result,TUInt) { \
    hpatch_StreamPos_t v=0; \
    check(hpatch_unpackUInt(src_code,src_code_end,&v)); \
    check(v==(TUInt)v);    \
    *(result)=(TUInt)v; }

bool _uncompress(const TByte* code,size_t codeLen,TByte* dst,size_t dstSize,hpatch_TDecompress* decompressPlugin);


static bool _openZipDiffData(TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin,
                  size_t* out_headInfoPos=0){
    #define kVersionTypeLen 7
    
    TByte buf[kVersionTypeLen + hpatch_kMaxCompressTypeLength+1+1];
    int readLen=sizeof(buf)-1;
    buf[readLen]='\0';
    if ((size_t)readLen>diffData->base.streamSize)
        readLen=(int)diffData->base.streamSize;
    check(readLen==diffData->base.read(diffData->base.streamHandle,0,buf,buf+readLen));
    //check type+version
    check(0==strncmp((const char*)buf,"ZiPat1&",kVersionTypeLen));
    {//read compressType
        check(decompressPlugin!=0);
        const char* compressType=(const char*)buf+kVersionTypeLen;
        size_t compressTypeLen=strlen(compressType);//safe
        check(compressTypeLen<=hpatch_kMaxCompressTypeLength);
        if (out_headInfoPos)
            *out_headInfoPos=kVersionTypeLen+compressTypeLen+1;
        hpatch_compressedDiffInfo compressedInfo={0,0,1};
        memcpy(compressedInfo.compressType,compressType,compressTypeLen+1); //'\0'
        return 0!=decompressPlugin->is_can_open(decompressPlugin,&compressedInfo);
    }
}

bool ZipDiffData_isCanDecompress(TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin){
    return _openZipDiffData(diffData,decompressPlugin);
}

#define _unpackIncList(list,count){ \
    uint32_t backValue=~(uint32_t)0;      \
    for (size_t i=0; i<count; ++i) {\
        uint32_t incValue=0;    \
        checkUnpackSize(&curBuf,bufEnd,&incValue,uint32_t); \
        backValue+=1+incValue;  \
        list[i]=backValue;      \
    }\
}

bool ZipDiffData_openRead(ZipDiffData* self,TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin){
    size_t  headDataSize=0;
    size_t  headDataCompressedSize=0;
    size_t  headDataPos=0;
    size_t  hdiffzSize=0;
    {
        size_t  headInoPos=0;
        check(_openZipDiffData(diffData,decompressPlugin,&headInoPos));
        //read head info
        TByte buf[hpatch_kMaxPackedUIntBytes*(17+3)];
        int readLen=sizeof(buf);
        if (headInoPos+readLen>diffData->base.streamSize)
            readLen=(int)(diffData->base.streamSize-headInoPos);
        check(readLen==diffData->base.read(diffData->base.streamHandle,headInoPos,buf,buf+readLen));
        //unpack head info
        const TByte* curBuf=buf;
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipFileCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipIsDataNormalized,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipAlignSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newCompressLevel,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newCompressMemLevel,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipCESize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->samePairCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newRefNotDecompressCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newRefCompressedSizeCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldZipIsDataNormalized,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldIsFileDataOffsetMatch,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldZipCESize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldRefCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldRefNotDecompressCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldCrc,uint32_t);
        checkUnpackSize(&curBuf,buf+readLen,&headDataSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&headDataCompressedSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&hdiffzSize,size_t);
        headDataPos=(curBuf-buf)+headInoPos;
        
        check(headDataCompressedSize <= diffData->base.streamSize-headDataPos);
        check(hdiffzSize <= diffData->base.streamSize-headDataPos-headDataCompressedSize);
    }
    {//head data
        //  memBuf used as:
        //  [     compressed headData   ]     [ uncompressed headData(packed list) ]
        //  [           unpacked_list        ]
        size_t memLeft=(self->samePairCount*2+self->newRefNotDecompressCount+self->newRefCompressedSizeCount
                        +self->oldRefCount+self->oldRefNotDecompressCount)*sizeof(uint32_t);
        if (headDataCompressedSize>memLeft) memLeft=_hpatch_align_upper(headDataCompressedSize,sizeof(uint32_t));
        assert(self->_buf==0);
        self->_buf=(TByte*)malloc(memLeft+headDataSize);
        check(self->_buf!=0);
        
        check((long)headDataCompressedSize==diffData->base.read(diffData->base.streamHandle,headDataPos,
                                                                self->_buf,self->_buf+headDataCompressedSize));
        check(_uncompress(self->_buf,headDataCompressedSize,self->_buf+memLeft,headDataSize,decompressPlugin));
        
        self->samePairList=(uint32_t*)self->_buf;
        self->newRefNotDecompressList=self->samePairList+self->samePairCount*2;
        self->newRefCompressedSizeList=self->newRefNotDecompressList+self->newRefNotDecompressCount;
        self->oldRefList=self->newRefCompressedSizeList+self->newRefCompressedSizeCount;
        self->oldRefNotDecompressList=self->oldRefList+self->oldRefCount;
        const TByte* curBuf=self->_buf+memLeft;
        const TByte* const bufEnd=self->_buf+memLeft+headDataSize;
        
        uint32_t backPairNew=~(uint32_t)0;
        hpatch_StreamPos_t backPairOld=~(hpatch_StreamPos_t)0;
        for (size_t i=0; i<self->samePairCount; ++i) {
            uint32_t curPairNew=0;
            checkUnpackSize(&curBuf,bufEnd,&curPairNew,uint32_t);
            backPairNew+=1+curPairNew;
            self->samePairList[i*2+0]=backPairNew;
            
            check(curBuf<bufEnd);
            TByte sign=(*curBuf)>>(8-1);
            hpatch_StreamPos_t incPairOld=0;
            check(hpatch_unpackUIntWithTag(&curBuf,bufEnd,&incPairOld,1));
            if (!sign)
                backPairOld+=1+incPairOld;
            else
                backPairOld=backPairOld+1-incPairOld;
            check(backPairOld==(uint32_t)backPairOld);
            self->samePairList[i*2+1]=(uint32_t)backPairOld;
        }
        _unpackIncList(self->newRefNotDecompressList,self->newRefNotDecompressCount);
        for (size_t i=0; i<self->newRefCompressedSizeCount; ++i) {
            checkUnpackSize(&curBuf,bufEnd,&self->newRefCompressedSizeList[i],uint32_t);
        }
        _unpackIncList(self->oldRefList,self->oldRefCount);
        _unpackIncList(self->oldRefNotDecompressList,self->oldRefNotDecompressCount);
        check(curBuf==bufEnd);
    }
    {//HDiffZ stream
        size_t hdiffzPos=headDataPos+headDataCompressedSize;
        self->_hdiffzData=*diffData;
        self->_hdiffzData.base.streamHandle=&self->_hdiffzData.base;
        TFileStreamInput_setOffset(&self->_hdiffzData,hdiffzPos);
        self->_hdiffzData.base.streamSize=hdiffzSize;
        self->_hdiffzData.m_fpos=(hpatch_StreamPos_t)(-1);
        self->hdiffzData=&self->_hdiffzData.base;
    }
    {//ExtraEdit stream
        //read size+tag
        check(diffData->base.streamSize>=4+kExtraEditLen);
        unsigned char buf4s[4+kExtraEditLen];
        check(4+kExtraEditLen==diffData->base.read(diffData->base.streamHandle,
                                                   diffData->base.streamSize-4-kExtraEditLen,buf4s,buf4s+4+kExtraEditLen));
        check(0==memcmp(kExtraEdit,buf4s+4,kExtraEditLen));//check tag
        uint32_t extraEditSize=readUInt32(buf4s);
        
        check(4+kExtraEditLen+self->_hdiffzData.m_offset+hdiffzSize+extraEditSize<=diffData->base.streamSize);
        hpatch_StreamPos_t extraEditPos=diffData->base.streamSize-extraEditSize-4-kExtraEditLen;
        self->_extraEdit=*diffData;
        self->_extraEdit.base.streamHandle=&self->_extraEdit.base;
        check(extraEditPos==(size_t)extraEditPos);
        TFileStreamInput_setOffset(&self->_extraEdit,(size_t)extraEditPos);
        self->_extraEdit.base.streamSize=extraEditSize;
        self->_extraEdit.m_fpos=(hpatch_StreamPos_t)(-1);
        self->extraEdit=&self->_extraEdit.base;
    }
    diffData->m_fpos=(hpatch_StreamPos_t)(-1); //force re seek
    
    return true;
}

bool _uncompress(const TByte* code,size_t codeLen,TByte* dst,size_t dstSize,hpatch_TDecompress* decompressPlugin){
    bool result=true;
    bool _isInClear=false;
    hpatch_TStreamInput codeStream;
    mem_as_hStreamInput(&codeStream,code,code+codeLen);
    hpatch_decompressHandle dec=decompressPlugin->open(decompressPlugin,dstSize,&codeStream,0,codeStream.streamSize);
    check_clear(dec!=0);
    check_clear((long)dstSize==decompressPlugin->decompress_part(decompressPlugin,dec,dst,dst+dstSize));
clear:
    _isInClear=true;
    decompressPlugin->close(decompressPlugin,dec);
    return result;
}
