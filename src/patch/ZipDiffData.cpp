//  ZipDiffData.cpp
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
#include "ZipDiffData.h"

#define kVersionTypeLen 8

void ZipDiffData_init(ZipDiffData* self){
    memset(self,0,sizeof(ZipDiffData));
}
void ZipDiffData_close(ZipDiffData* self){
    if (self->_buf) { free(self->_buf); self->_buf=0; }
}

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; assert(false); if (!_isInClear){ goto clear; } } }
#define checkUnpackSize(src_code,src_code_end,result,TUInt) { \
    hpatch_StreamPos_t v=0; \
    check(hpatch_unpackUInt(src_code,src_code_end,&v)); \
    check(v==(TUInt)v);    \
    *(result)=(TUInt)v; }

bool _uncompress(const TByte* code,size_t codeLen,TByte* dst,size_t dstSize,hpatch_TDecompress* decompressPlugin);


bool ZipDiffData_open(ZipDiffData* self,TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin){
    bool result=true;
    bool _isInClear=false;
    hpatch_StreamPos_t  headDataSize=0;
    hpatch_StreamPos_t  headDataCompressedSize=0;
    size_t              headDataPos=0;
    TByte buf[kVersionTypeLen+hpatch_kMaxPackedUIntBytes*6];
    int readLen=sizeof(buf);
    {//type+version
        if (readLen>diffData->base.streamSize)
            readLen=(int)diffData->base.streamSize;
        check(readLen==diffData->base.read(diffData->base.streamHandle,0,buf,buf+readLen));
        check(0==strncmp((const char*)buf,"ZipDiff1",kVersionTypeLen));
    }
    {//head
        hpatch_StreamPos_t hdiffzSize=0;
        const TByte* curBuf=buf+kVersionTypeLen;
        checkUnpackSize(&curBuf,buf+readLen,&self->refCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->samePairCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->CHeadEqBitCount,size_t);
        check(hpatch_unpackUInt(&curBuf,buf+readLen,&hdiffzSize));
        check(hpatch_unpackUInt(&curBuf,buf+readLen,&headDataSize));
        check(hpatch_unpackUInt(&curBuf,buf+readLen,&headDataCompressedSize));
        headDataPos=(curBuf-buf);
        
        check(headDataCompressedSize <= diffData->base.streamSize-headDataPos);
        check(hdiffzSize == diffData->base.streamSize-headDataPos-headDataCompressedSize);
    }
    {//head data
        //  memBuf used as:
        //  [  compressed headData  ]        [ uncompressed headData(CHeadEqList+packed(refList+samePairList)) ]
        //  [   refList   +   samePairList  ]
        size_t CHeadEqSize=(self->CHeadEqBitCount+7)/8;
        size_t memLeft=self->refCount*sizeof(uint32_t) + self->samePairCount*2*sizeof(uint32_t);
        if (headDataCompressedSize>memLeft) memLeft=headDataCompressedSize;
        TByte* memBuf=(TByte*)malloc(memLeft+headDataSize);
        TByte* memBuf_end=memBuf+memLeft+headDataSize;
        check(memBuf!=0);
        assert(self->_buf==0);
        self->_buf=memBuf;
        self->refList=(uint32_t*)memBuf;
        self->samePairList=(uint32_t*)(memBuf+self->refCount*sizeof(uint32_t));
        self->CHeadEqList=memBuf+memLeft;
        
        check(headDataCompressedSize==diffData->base.read(diffData->base.streamHandle,headDataPos,
                                                          memBuf,memBuf+headDataCompressedSize));
        check(_uncompress(memBuf,headDataCompressedSize,memBuf+memLeft,headDataSize,decompressPlugin));
        
        const TByte* curBuf=memBuf+memLeft+CHeadEqSize;
        uint32_t backRef=0;
        for (size_t i=0; i<self->refCount; ++i) {
            uint32_t curRef=0;
            checkUnpackSize(&curBuf,memBuf_end,&curRef,uint32_t);
            curRef=backRef+1+curRef;
            self->refList[i]=curRef;
            backRef=curRef;
        }
        uint32_t backPairNew=0;
        hpatch_StreamPos_t backPairOld=0;
        for (size_t i=0; i<self->samePairCount; ++i) {
            uint32_t curPairNew=0;
            checkUnpackSize(&curBuf,memBuf_end,&curPairNew,uint32_t);
            curPairNew=backPairNew+1+curPairNew;
            self->samePairList[i*2+0]=curPairNew;
            backPairNew=curPairNew;
            
            check(curBuf<memBuf_end);
            TByte sign=(*curBuf)>>(8-1);
            hpatch_StreamPos_t curPairOld=0;
            check(hpatch_unpackUIntWithTag(&curBuf,memBuf_end,&curPairOld,1));
            if (!sign)
                curPairOld=backPairOld+1+curPairOld;
            else
                curPairOld=backPairOld+1-curPairOld;
            check(curPairOld==(uint32_t)curPairOld);
            self->samePairList[i*2+1]=(uint32_t)curPairOld;
            backPairOld=curPairOld;
        }
        check(curBuf==memBuf_end);
    }
    {//HDiffZ stream
        hpatch_StreamPos_t hdiffzPos=headDataPos+headDataCompressedSize;
        self->_hdiffzData=*diffData;
        TFileStreamInput_setOffset(&self->_hdiffzData,hdiffzPos);
        self->hdiffzData=&self->_hdiffzData.base;
    }
    
clear:
    _isInClear=true;
    return result;
}

bool _uncompress(const TByte* code,size_t codeLen,TByte* dst,size_t dstSize,hpatch_TDecompress* decompressPlugin){
    bool result=true;
    bool _isInClear=false;
    hpatch_TStreamInput codeStream;
    mem_as_hStreamInput(&codeStream,code,code+codeLen);
    hpatch_decompressHandle dec=decompressPlugin->open(decompressPlugin,dstSize,&codeStream,0,codeStream.streamSize);
    check(dec!=0);
    check(dstSize==decompressPlugin->decompress_part(decompressPlugin,dec,dst,dst+dstSize));
clear:
    _isInClear=true;
    decompressPlugin->close(decompressPlugin,dec);
    return result;
}
