//  NewStream.cpp
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
#include "NewStream.h"

void NewStream_init(NewStream* self){
    memset(self,0,sizeof(NewStream)-sizeof(UnZipper));
    UnZipper_init(&self->_newZipVCE);
}
void NewStream_close(NewStream* self){
    bool ret=UnZipper_close(&self->_newZipVCE);
    assert(ret);
}

static bool _copy_same_file(NewStream* self,uint32_t newFileIndex,uint32_t oldFileIndex);
static bool _file_entry_end(NewStream* self);

inline static void _update_compressedSize(NewStream* self,int newFileIndex,uint32_t compressedSize){
    self->_newZipVCE._fileCompressedSizes[newFileIndex]=compressedSize;
}

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return 0; } }

static long _NewStream_write(hpatch_TStreamOutputHandle streamHandle,
                             const hpatch_StreamPos_t _writeToPos,
                             const unsigned char* data,const unsigned char* data_end){
    long result=(long)(data_end-data);
    hpatch_StreamPos_t writeToPos=_writeToPos;
    NewStream* self=(NewStream*)streamHandle;
    check(!self->isFinish);
    assert(writeToPos<self->_curWriteToPosEnd);
    if (writeToPos+result>self->_curWriteToPosEnd){
        long leftLen=(long)(self->_curWriteToPosEnd-writeToPos);
        check(leftLen==_NewStream_write(streamHandle,writeToPos,data,data+leftLen));
        check((result-leftLen)==_NewStream_write(streamHandle,writeToPos+leftLen,data+leftLen,data_end));
        return result;
    }
    
    //write data
    if (self->_curFileIndex<0){//vce
        memcpy(self->_newZipVCE._cache_vce+writeToPos,data,result);
    }else{
        check(self->_curFileIndex<self->_fileCount);
        check(Zipper_file_append_part(self->_out_newZip,data,result));
    }
    
    if (writeToPos+result<self->_curWriteToPosEnd)//write continue
        return result;
    
    //write one end
    if (self->_curFileIndex<0){//vce ok
        check(UnZipper_updateVCE(&self->_newZipVCE,self->_newZipIsDataNormalized));
        bool newIsStable=self->_newZipVCE._isDataNormalized && UnZipper_isHaveApkV2Sign(&self->_newZipVCE);
        bool oldIsStable=self->_oldZip->_isDataNormalized && UnZipper_isHaveApkV2Sign(self->_oldZip);
        self->_isAlwaysReCompress=newIsStable&(!oldIsStable);
    }else{
        check(Zipper_file_append_end(self->_out_newZip));
    }
    ++self->_curFileIndex;
    
    while ((self->_curFileIndex<self->_fileCount)&&(self->_curSamePairIndex<self->_samePairCount)) {
        uint32_t newFileIndex=self->_samePairList[self->_curSamePairIndex*2];
        if (self->_curFileIndex!=newFileIndex) break;
        uint32_t oldFileIndex=self->_samePairList[self->_curSamePairIndex*2+1];
        check(_copy_same_file(self,newFileIndex,oldFileIndex));
        ++self->_curSamePairIndex;
        ++self->_curFileIndex;
    }
    
    if (self->_curFileIndex<self->_fileCount){//open file for write
        ZipFilePos_t uncompressedSize=UnZipper_file_uncompressedSize(&self->_newZipVCE,self->_curFileIndex);
        ZipFilePos_t compressedSize=uncompressedSize;
        if (UnZipper_file_isCompressed(&self->_newZipVCE,self->_curFileIndex)){
            check(self->_curNewReCompressSizeIndex<self->_newReCompressSizeCount);
            compressedSize=self->_newReCompressSizeList[self->_curNewReCompressSizeIndex];
            ++self->_curNewReCompressSizeIndex;
        }
        _update_compressedSize(self,self->_curFileIndex,compressedSize);
        
        bool isWriteCompressedData=(self->_curNewRefNotDecompressIndex<self->_newRefNotDecompressCount)
                &&(self->_newRefNotDecompressList[self->_curNewRefNotDecompressIndex]==self->_curFileIndex);
        if (isWriteCompressedData)
            ++self->_curNewRefNotDecompressIndex;
        
        check(Zipper_file_append_begin(self->_out_newZip,&self->_newZipVCE,self->_curFileIndex,
                                       isWriteCompressedData,uncompressedSize,compressedSize));
        self->_curWriteToPosEnd+=isWriteCompressedData?compressedSize:uncompressedSize;
        return result;
    }
    
    //file entry end
    check(_file_entry_end(self));
    return result;
}

#undef   check
#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }

bool NewStream_open(NewStream* self,Zipper* out_newZip,UnZipper* oldZip,
                    size_t newDataSize,bool newZipIsDataNormalized,size_t newZipVCESize,
                    const uint32_t* samePairList,size_t samePairCount,
                    uint32_t* newRefNotDecompressList,size_t newRefNotDecompressCount,
                    const uint32_t* reCompressList,size_t reCompressCount){
    assert(self->_out_newZip==0);
    self->isFinish=false;
    self->_out_newZip=out_newZip;
    self->_oldZip=oldZip;
    self->_newZipIsDataNormalized=newZipIsDataNormalized;
    self->_samePairList=samePairList;
    self->_samePairCount=samePairCount;
    self->_newRefNotDecompressList=newRefNotDecompressList;
    self->_newRefNotDecompressCount=newRefNotDecompressCount;
    self->_newReCompressSizeList=reCompressList;
    self->_newReCompressSizeCount=reCompressCount;
    self->_fileCount=out_newZip->_fileEntryMaxCount;
    
    self->_stream.streamHandle=self;
    self->_stream.streamSize=newDataSize;
    self->_stream.write=_NewStream_write;
    self->stream=&self->_stream;
    
    check(UnZipper_openForVCE(&self->_newZipVCE,(ZipFilePos_t)newZipVCESize,self->_fileCount));
    
    self->_curFileIndex=-1;
    self->_curWriteToPosEnd=newZipVCESize;
    self->_curSamePairIndex=0;
    self->_curNewRefNotDecompressIndex=0;
    self->_curNewReCompressSizeIndex=0;
    self->_isAlwaysReCompress=false;
    return true;
}

static bool _file_entry_end(NewStream* self){
    check(!self->isFinish);
    check(self->_curFileIndex==self->_fileCount);
    check(self->_curSamePairIndex==self->_samePairCount);
    check(self->_curNewRefNotDecompressIndex==self->_newRefNotDecompressCount);
    check(self->_curNewReCompressSizeIndex==self->_newReCompressSizeCount);
    
    if (UnZipper_isHaveApkV2Sign(&self->_newZipVCE)){
        check(Zipper_copyApkV2Sign_before_fileHeader(self->_out_newZip,&self->_newZipVCE));
    }
    for (int i=0; i<self->_fileCount; ++i) {
        check(Zipper_fileHeader_append(self->_out_newZip,&self->_newZipVCE,i));
    }
    check(Zipper_endCentralDirectory_append(self->_out_newZip,&self->_newZipVCE));
    self->isFinish=true;
    return true;
}

bool _copy_same_file(NewStream* self,uint32_t newFileIndex,uint32_t oldFileIndex){
    uint32_t uncompressedSize=UnZipper_file_uncompressedSize(self->_oldZip,oldFileIndex);
    check(UnZipper_file_uncompressedSize(&self->_newZipVCE,newFileIndex)==uncompressedSize);
    check(UnZipper_file_crc32(&self->_newZipVCE,newFileIndex)==UnZipper_file_crc32(self->_oldZip,oldFileIndex));
    uint32_t compressedSize=UnZipper_file_compressedSize(self->_oldZip,oldFileIndex);
    bool newIsCompress=UnZipper_file_isCompressed(&self->_newZipVCE,newFileIndex);
    bool oldIsCompress=UnZipper_file_isCompressed(self->_oldZip,oldFileIndex);
    bool isNeedDecompress=self->_isAlwaysReCompress|((oldIsCompress)&(!newIsCompress));
    _update_compressedSize(self,newFileIndex,newIsCompress?compressedSize:uncompressedSize);
    bool  appendDataIsCompressed=(!isNeedDecompress)&&oldIsCompress;
    check(Zipper_file_append_begin(self->_out_newZip,&self->_newZipVCE,newFileIndex,
                                   appendDataIsCompressed,uncompressedSize,compressedSize));
    const hpatch_TStreamOutput* outStream=Zipper_file_append_part_as_stream(self->_out_newZip);
    if (isNeedDecompress){
        check(UnZipper_fileData_decompressTo(self->_oldZip,oldFileIndex,outStream));
    }else{
        check(UnZipper_fileData_copyTo(self->_oldZip,oldFileIndex,outStream));
    }
    check(Zipper_file_append_end(self->_out_newZip));
    return true;
}


