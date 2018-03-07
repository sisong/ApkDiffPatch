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

static const char* kVersionType="ZipDiff1";
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

bool ZipDiffData_openRead(ZipDiffData* self,TFileStreamInput* diffData,hpatch_TDecompress* decompressPlugin){
    bool result=true;
    bool _isInClear=false;
    hpatch_StreamPos_t  headDataSize=0;
    hpatch_StreamPos_t  headDataCompressedSize=0;
    size_t              headDataPos=0;
    {
        //read head
        TByte buf[kVersionTypeLen+hpatch_kMaxPackedUIntBytes*(9+3)];
        int readLen=sizeof(buf);
        if (readLen>diffData->base.streamSize)
            readLen=(int)diffData->base.streamSize;
        check(readLen==diffData->base.read(diffData->base.streamHandle,0,buf,buf+readLen));
        //check type+version
        check(0==strncmp((const char*)buf,kVersionType,kVersionTypeLen));
        //unpack head info
        hpatch_StreamPos_t hdiffzSize=0;
        const TByte* curBuf=buf+kVersionTypeLen;
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipFileCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipVCESize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->reCompressCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->samePairCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldZipVCESize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->refCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->refSumSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldCrc,uint32_t);
        check(hpatch_unpackUInt(&curBuf,buf+readLen,&headDataSize));
        check(hpatch_unpackUInt(&curBuf,buf+readLen,&headDataCompressedSize));
        check(hpatch_unpackUInt(&curBuf,buf+readLen,&hdiffzSize));
        headDataPos=(curBuf-buf);
        
        check(headDataCompressedSize <= diffData->base.streamSize-headDataPos);
        check(hdiffzSize == diffData->base.streamSize-headDataPos-headDataCompressedSize);
    }
    {//head data
        //  memBuf used as:
        //  [     compressed headData   ]     [ uncompressed headData(packed list) ]
        //  [           unpacked_list        ]
        size_t memLeft=(self->reCompressCount+self->refCount+self->samePairCount*2)*sizeof(uint32_t);
        if (headDataCompressedSize>memLeft) memLeft=_hpatch_align_upper(headDataCompressedSize,sizeof(uint32_t));
        assert(self->_buf==0);
        self->_buf=(TByte*)malloc(memLeft+headDataSize);
        check(self->_buf!=0);
        
        check(headDataCompressedSize==diffData->base.read(diffData->base.streamHandle,headDataPos,
                                                          self->_buf,self->_buf+headDataCompressedSize));
        check(_uncompress(self->_buf,headDataCompressedSize,self->_buf+memLeft,headDataSize,decompressPlugin));
        
        self->reCompressList=(uint32_t*)self->_buf;
        self->refList=self->reCompressList+self->reCompressCount;
        self->samePairList=self->refList+self->refCount;
        const TByte* curBuf=self->_buf+memLeft;
        const TByte* const bufEnd=self->_buf+memLeft+headDataSize;
        
        for (size_t i=0; i<self->reCompressCount; ++i) {
            checkUnpackSize(&curBuf,bufEnd,&self->reCompressList[i],uint32_t);
        }
        
        uint32_t backValue=-1;
        for (size_t i=0; i<self->refCount; ++i) {
            uint32_t incValue=0;
            checkUnpackSize(&curBuf,bufEnd,&incValue,uint32_t);
            backValue+=1+incValue;
            self->refList[i]=backValue;
        }
    
        uint32_t backPairNew=-1;
        hpatch_StreamPos_t backPairOld=-1;
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
        check(curBuf==bufEnd);
    }
    {//HDiffZ stream
        hpatch_StreamPos_t hdiffzPos=headDataPos+headDataCompressedSize;
        self->_hdiffzData=*diffData;
        self->_hdiffzData.base.streamHandle=&self->_hdiffzData.base;
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
