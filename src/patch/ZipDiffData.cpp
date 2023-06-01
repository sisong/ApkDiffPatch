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


static bool _openZipDiffData(const hpatch_TStreamInput* diffData,hpatch_TDecompress* decompressPlugin,
                  size_t* out_headInfoPos=0){
    const char* kVersionType="ZiPat1&";
    const size_t kVersionTypeLen=7;
    assert(kVersionTypeLen==strlen(kVersionType));
    
    TByte buf[kVersionTypeLen + hpatch_kMaxPluginTypeLength+1+1];
    int readLen=sizeof(buf)-1;
    buf[readLen]='\0';
    if ((size_t)readLen>diffData->streamSize)
        readLen=(int)diffData->streamSize;
    check(diffData->read(diffData,0,buf,buf+readLen));
    //check type+version
    check(0==strncmp((const char*)buf,kVersionType,kVersionTypeLen));
    {//read compressType
        check(decompressPlugin!=0);
        const char* compressType=(const char*)buf+kVersionTypeLen;
        size_t compressTypeLen=strlen(compressType);//safe
        check(compressTypeLen<=hpatch_kMaxPluginTypeLength);
        if (out_headInfoPos)
            *out_headInfoPos=kVersionTypeLen+compressTypeLen+1;
        hpatch_compressedDiffInfo compressedInfo={0,0,1};
        memcpy(compressedInfo.compressType,compressType,compressTypeLen+1); //'\0'
        return 0!=decompressPlugin->is_can_open(compressedInfo.compressType);
    }
}

bool ZipDiffData_isCanDecompress(const hpatch_TStreamInput* diffData,hpatch_TDecompress* decompressPlugin){
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

bool ZipDiffData_openRead(ZipDiffData* self,const hpatch_TStreamInput* diffData,
                          hpatch_TDecompress* decompressPlugin){
    size_t  headDataSize=0;
    size_t  headDataCompressedSize=0;
    size_t  headDataPos=0;
    size_t  hdiffzSize=0;
    {
        size_t  headInoPos=0;
        check(_openZipDiffData(diffData,decompressPlugin,&headInoPos));
        //read head info
        TByte buf[hpatch_kMaxPackedUIntBytes*(16+3)];
        int readLen=sizeof(buf);
        if (headInoPos+readLen>diffData->streamSize)
            readLen=(int)(diffData->streamSize-headInoPos);
        check(diffData->read(diffData,headInoPos,buf,buf+readLen));
        //unpack head info
        const TByte* curBuf=buf;
        checkUnpackSize(&curBuf,buf+readLen,&self->PatchModel,size_t);
        check(self->PatchModel<=1);//now must 0 or 1
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipFileCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipIsDataNormalized,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipAlignSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newCompressLevel,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newCompressMemLevel,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newOtherCompressLevel,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newOtherCompressMemLevel,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newZipCESize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->samePairCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newRefOtherCompressedCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->newRefCompressedSizeCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldZipIsDataNormalized,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldIsFileDataOffsetMatch,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldZipCESize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldRefCount,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&self->oldCrc,uint32_t);
        checkUnpackSize(&curBuf,buf+readLen,&headDataSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&headDataCompressedSize,size_t);
        checkUnpackSize(&curBuf,buf+readLen,&hdiffzSize,size_t);
        headDataPos=(curBuf-buf)+headInoPos;
        
        check(headDataCompressedSize <= diffData->streamSize-headDataPos);
        check(hdiffzSize <= diffData->streamSize-headDataPos-headDataCompressedSize);
    }
    {//head data
        //  memBuf used as:
        //  [     compressed headData   ]     [ uncompressed headData(packed list) ]
        //  [           unpacked_list        ]
        size_t memLeft=(self->samePairCount*2+self->newRefOtherCompressedCount
                        +self->newRefCompressedSizeCount+self->oldRefCount)*sizeof(uint32_t);
        if (headDataCompressedSize>memLeft) memLeft=_hpatch_align_upper(headDataCompressedSize,sizeof(uint32_t));
        assert(self->_buf==0);
        self->_buf=(TByte*)malloc(memLeft+headDataSize);
        check(self->_buf!=0);
        
        check(diffData->read(diffData,headDataPos,self->_buf,self->_buf+headDataCompressedSize));
        if (headDataSize>0){
            check(hpatch_deccompress_mem(decompressPlugin,self->_buf,self->_buf+headDataCompressedSize,
                                         self->_buf+memLeft,self->_buf+memLeft+headDataSize));
        }
        
        self->samePairList=(uint32_t*)self->_buf;
        self->newRefOtherCompressedList=self->samePairList+self->samePairCount*2;
        self->newRefCompressedSizeList=self->newRefOtherCompressedList+self->newRefOtherCompressedCount;
        self->oldRefList=self->newRefCompressedSizeList+self->newRefCompressedSizeCount;
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
        _unpackIncList(self->newRefOtherCompressedList,self->newRefOtherCompressedCount);
        for (size_t i=0; i<self->newRefCompressedSizeCount; ++i) {
            checkUnpackSize(&curBuf,bufEnd,&self->newRefCompressedSizeList[i],uint32_t);
        }
        _unpackIncList(self->oldRefList,self->oldRefCount);
        check(curBuf==bufEnd);
    }
    {//HDiffZ stream
        size_t hdiffzPos=headDataPos+headDataCompressedSize;
        TStreamInputClip_init(&self->_hdiffzData,diffData,hdiffzPos,hdiffzPos+hdiffzSize);
        self->hdiffzData=&self->_hdiffzData.base;
    }
    {//ExtraEdit stream
        //read size+tag
        check(diffData->streamSize>=4+kExtraEditLen);
        unsigned char buf4s[4+kExtraEditLen];
        check(diffData->read(diffData,diffData->streamSize-4-kExtraEditLen,buf4s,buf4s+4+kExtraEditLen));
        check(0==memcmp(kExtraEdit,buf4s+4,kExtraEditLen));//check tag
        uint32_t extraEditSize=readUInt32(buf4s);

        check(4+kExtraEditLen+self->_hdiffzData.clipBeginPos+hdiffzSize+extraEditSize<=diffData->streamSize);
        hpatch_StreamPos_t extraEditPos=diffData->streamSize-extraEditSize-4-kExtraEditLen;
        TStreamInputClip_init(&self->_extraEdit,diffData,extraEditPos,extraEditPos+extraEditSize);
        self->extraEdit=&self->_extraEdit.base;
    }
    return true;
}
