//  NewStream.cpp
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
#include "NewStream.h"

void NewStream_init(NewStream* self){
    memset(self,0,sizeof(NewStream));
}
void NewStream_close(NewStream* self){
    bool ret=UnZipper_close(&self->_newZipVCE);
    if (!ret) assert(ret);
}

static bool _copy_same_file(NewStream* self,uint32_t newFileIndex,uint32_t oldFileIndex);
static bool _file_entry_end(NewStream* self);

inline static void _update_compressedSize(NewStream* self,int newFileIndex,uint32_t compressedSize){
    self->_newZipVCE._fileCompressedSizes[newFileIndex]=compressedSize;
}

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return 0; } }

static hpatch_BOOL _NewStream_write(const hpatch_TStreamOutput* stream,
                                    const hpatch_StreamPos_t _writeToPos,
                                    const unsigned char* data,const unsigned char* data_end){
    size_t dataSize=(size_t)(data_end-data);
    hpatch_StreamPos_t writeToPos=_writeToPos;
    NewStream* self=(NewStream*)stream->streamImport;
    check(!self->isFinish);
    assert(dataSize>0);
    assert(writeToPos<self->_curWriteToPosEnd);
    if (writeToPos+dataSize>self->_curWriteToPosEnd){
        size_t leftLen=(size_t)(self->_curWriteToPosEnd-writeToPos);
        check(_NewStream_write(stream,writeToPos,data,data+leftLen));
        check(_NewStream_write(stream,writeToPos+leftLen,data+leftLen,data_end));
        return hpatch_TRUE;
    }
    
    //write data
    if (self->_curFileIndex<0){//fvce
        memcpy(self->_newZipVCE._cache_fvce+self->_extraEditSize+writeToPos,data,dataSize);
    }else{
        check(self->_curFileIndex<self->_fileCount);
#if (_IS_NEED_VIRTUAL_ZIP)
        if ((self->_vout)&&(self->_vout->virtualStream)){
            check(Zipper_file_append_part(self->_out_newZip,data,dataSize)); //todo:
        }else
#endif
            check(Zipper_file_append_part(self->_out_newZip,data,dataSize));
    }
    
    if (writeToPos+dataSize<self->_curWriteToPosEnd)//write continue
        return hpatch_TRUE;
    
    //write one end
    if (self->_curFileIndex<0){//vce ok
        check(UnZipper_updateVirtualVCE(&self->_newZipVCE,self->_newZipIsDataNormalized,self->_newZipCESize));
        bool newIsStable=self->_newZipVCE._isDataNormalized && UnZipper_isHaveApkV2Sign(&self->_newZipVCE);
        bool oldIsStable=self->_oldZip->_isDataNormalized && UnZipper_isHaveApkV2Sign(self->_oldZip);
        self->_isAlwaysReCompress=newIsStable&(!oldIsStable);
        //if (newIsStable)
            Zipper_by_multi_thread(self->_out_newZip,self->_threadNum);
    }else{
        check(Zipper_file_append_end(self->_out_newZip));
#if (_IS_NEED_VIRTUAL_ZIP)
        if ((self->_vout)&&(self->_vout->virtualStream))
            check(self->_vout->endVirtual(self->_vout));
#endif
    }
    ++self->_curFileIndex;
    
    while (self->_curFileIndex<self->_fileCount) {
        if ((self->_curSamePairIndex<self->_samePairCount)
            &&(self->_curFileIndex==(int)self->_samePairList[self->_curSamePairIndex*2])){
            const uint32_t* pairNewiOldi=self->_samePairList+self->_curSamePairIndex*2;
            check(_copy_same_file(self,pairNewiOldi[0],pairNewiOldi[1]));
            ++self->_curSamePairIndex;
            ++self->_curFileIndex;
        }else if ((0==UnZipper_file_uncompressedSize(&self->_newZipVCE,self->_curFileIndex))
                  &&(!UnZipper_file_isCompressed(&self->_newZipVCE,self->_curFileIndex))){
            _update_compressedSize(self,self->_curFileIndex,0);
#if (_IS_NEED_VIRTUAL_ZIP)
            if (self->_vout){
                TVirtualZip_out_type ty=self->_vout->beginVirtual(self->_vout,&self->_newZipVCE,self->_curFileIndex);
                switch (ty) {
                    case kVirtualZip_out_void: { //ok, do nothing
                        assert(self->_vout->virtualStream==0); } break;
                    case kVirtualZip_out_emptyFile_cast: { //ok, already fileSize==0, do nothing
                        assert(self->_vout->virtualStream==0); } break;
                    case kVirtualZip_out_emptyFile_uncompressed: { //already fileSize==0, need endVirtual
                        assert(self->_vout->virtualStream!=0); } break;
                    default: { check(false); } break; //error, or unknow as error
                }
            }
#endif
            check(Zipper_file_append_begin(self->_out_newZip,&self->_newZipVCE,self->_curFileIndex,false,0,0));
            check(Zipper_file_append_end(self->_out_newZip));
#if (_IS_NEED_VIRTUAL_ZIP)
            if ((self->_vout)&&(self->_vout->virtualStream))
                check(self->_vout->endVirtual(self->_vout));
#endif
            ++self->_curFileIndex;
        }else{
            break;
        }
    }
    
    if (self->_curFileIndex<self->_fileCount){//open file for write
#if (_IS_NEED_VIRTUAL_ZIP)
        if (self->_vout){
            //todo:
            TVirtualZip_out_type ty=self->_vout->beginVirtual(self->_vout,&self->_newZipVCE,self->_curFileIndex);
            switch (ty) {
                case kVirtualZip_out_void: { //ok, do nothing
                    assert(self->_vout->virtualStream==0); } break;
                case kVirtualZip_out_emptyFile_cast: { //ok, already fileSize==0, do nothing
                    assert(self->_vout->virtualStream==0); } break;
                case kVirtualZip_out_emptyFile_uncompressed: { //already fileSize==0, need endVirtual
                    assert(self->_vout->virtualStream!=0); } break;
                default: { check(false); } break; //error, or unknow as error
            }
        }
#endif
        ZipFilePos_t uncompressedSize=UnZipper_file_uncompressedSize(&self->_newZipVCE,self->_curFileIndex);
        ZipFilePos_t compressedSize=uncompressedSize;
        if (UnZipper_file_isCompressed(&self->_newZipVCE,self->_curFileIndex)){
            check(self->_curNewReCompressSizeIndex<self->_newReCompressSizeCount);
            compressedSize=self->_newReCompressSizeList[self->_curNewReCompressSizeIndex];
            ++self->_curNewReCompressSizeIndex;
        }
        _update_compressedSize(self,self->_curFileIndex,compressedSize);
        
        bool isWriteOtherCompressedData=(self->_curNewOtherCompressIndex<self->_newRefOtherCompressedCount)
                &&((int)self->_newRefOtherCompressedList[self->_curNewOtherCompressIndex]==self->_curFileIndex);
        if (isWriteOtherCompressedData)
            ++self->_curNewOtherCompressIndex;
        if (isWriteOtherCompressedData && self->_newOtherCompressIsValid){
            check(Zipper_file_append_beginWith(self->_out_newZip,&self->_newZipVCE,self->_curFileIndex,
                                               false,uncompressedSize,compressedSize,
                                               self->_newOtherCompressLevel,self->_newOtherCompressMemLevel));
            self->_curWriteToPosEnd+=uncompressedSize;
        }else{
            check(Zipper_file_append_begin(self->_out_newZip,&self->_newZipVCE,self->_curFileIndex,
                                           isWriteOtherCompressedData,uncompressedSize,compressedSize));
            self->_curWriteToPosEnd+=isWriteOtherCompressedData?compressedSize:uncompressedSize;
        }
        return hpatch_TRUE;
    }
    
    //file entry end
    check(_file_entry_end(self));
    return hpatch_TRUE;
}

#undef   check
#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }

bool NewStream_open(NewStream* self,Zipper* out_newZip,UnZipper* oldZip,
                    size_t newDataSize,bool newZipIsDataNormalized,
                    size_t newZipCESize,const hpatch_TStreamInput* extraEdit,
                    const uint32_t* samePairList,size_t samePairCount,
                    uint32_t* newRefOtherCompressedList,size_t newRefOtherCompressedCount,
                    int newOtherCompressLevel,int newOtherCompressMemLevel,
                    const uint32_t* reCompressList,size_t reCompressCount,
                    int threadNum _VIRTUAL_OUT_D(virtual_out)){
    assert(self->_out_newZip==0);
    self->isFinish=false;
    self->_out_newZip=out_newZip;
    self->_oldZip=oldZip;
    self->_newZipIsDataNormalized=newZipIsDataNormalized;
    self->_newZipCESize=newZipCESize;
    self->_samePairList=samePairList;
    self->_samePairCount=samePairCount;
    self->_newRefOtherCompressedList=newRefOtherCompressedList;
    self->_newRefOtherCompressedCount=newRefOtherCompressedCount;
    self->_newOtherCompressIsValid=(newOtherCompressLevel|newOtherCompressMemLevel)!=0;
    self->_newOtherCompressLevel=newOtherCompressLevel;
    self->_newOtherCompressMemLevel=newOtherCompressMemLevel;
    self->_newReCompressSizeList=reCompressList;
    self->_newReCompressSizeCount=reCompressCount;
    self->_fileCount=out_newZip->_fileEntryMaxCount;
    self->_threadNum=threadNum;
    
    self->_stream.streamImport=self;
    self->_stream.streamSize=newDataSize;
    self->_stream.write=_NewStream_write;
    self->stream=&self->_stream;
    
    self->_extraEditSize=(size_t)extraEdit->streamSize;
    check(UnZipper_openVirtualVCE(&self->_newZipVCE,(ZipFilePos_t)(newZipCESize+self->_extraEditSize),self->_fileCount));
    if (self->_extraEditSize>0){
        check(extraEdit->read(extraEdit,0,self->_newZipVCE._cache_fvce,
                              self->_newZipVCE._cache_fvce+self->_extraEditSize));
    }
    
    self->_curFileIndex=-1;
    self->_curWriteToPosEnd=newZipCESize;
    self->_curSamePairIndex=0;
    self->_curNewOtherCompressIndex=0;
    self->_curNewReCompressSizeIndex=0;
    self->_isAlwaysReCompress=false;
#if (_IS_NEED_VIRTUAL_ZIP)
    self->_vout=virtual_out;
#endif
    return true;
}

static bool _file_entry_end(NewStream* self){
    check(!self->isFinish);
    check(self->_curFileIndex==self->_fileCount);
    check(self->_curSamePairIndex==self->_samePairCount);
    check(self->_curNewOtherCompressIndex==self->_newRefOtherCompressedCount);
    check(self->_curNewReCompressSizeIndex==self->_newReCompressSizeCount);
    
    check(Zipper_copyExtra_before_fileHeader(self->_out_newZip,&self->_newZipVCE));
    
    for (int i=0; i<self->_fileCount; ++i) {
        check(Zipper_fileHeader_append(self->_out_newZip,&self->_newZipVCE,i));
    }
    check(Zipper_endCentralDirectory_append(self->_out_newZip,&self->_newZipVCE));
    self->isFinish=true;
    return true;
}

static bool _copy_same_file(NewStream* self,uint32_t newFileIndex,uint32_t oldFileIndex){
    uint32_t uncompressedSize=UnZipper_file_uncompressedSize(self->_oldZip,oldFileIndex);
    check(UnZipper_file_uncompressedSize(&self->_newZipVCE,newFileIndex)==uncompressedSize);
    check(UnZipper_file_crc32(&self->_newZipVCE,newFileIndex)==UnZipper_file_crc32(self->_oldZip,oldFileIndex));
    uint32_t compressedSize=UnZipper_file_compressedSize(self->_oldZip,oldFileIndex);
    bool newIsCompress=UnZipper_file_isCompressed(&self->_newZipVCE,newFileIndex);
    bool oldIsCompress=UnZipper_file_isCompressed(self->_oldZip,oldFileIndex);
    bool isNeedDecompress=self->_isAlwaysReCompress|((oldIsCompress)&(!newIsCompress));
    _update_compressedSize(self,newFileIndex,newIsCompress?compressedSize:uncompressedSize);
    bool  appendDataIsCompressed=(!isNeedDecompress)&&oldIsCompress;
    
#if (_IS_NEED_VIRTUAL_ZIP)
    TVirtualZip_out_type ty=kVirtualZip_out_void;
    if (self->_vout){
        ty=self->_vout->beginVirtualCopy(self->_vout,&self->_newZipVCE,newFileIndex,
                                         self->_oldZip,oldFileIndex);
        switch (ty) {
            case kVirtualZip_out_void: { //ok, do nothing
                assert(self->_vout->virtualStream==0); } break;
            case kVirtualZip_out_emptyFile_cast: { // set fileSize==0
                assert(self->_vout->virtualStream==0);
                uncompressedSize=0; compressedSize=0; } break;
            case kVirtualZip_out_emptyFile_uncompressed: { //set fileSize==0, need out and endVirtual
                assert(self->_vout->virtualStream!=0);
                uncompressedSize=0; compressedSize=0;
                isNeedDecompress=true; } break;
            default: { check(false); } break; //error, or unknow as error
        }
    }
#endif
    check(Zipper_file_append_begin(self->_out_newZip,&self->_newZipVCE,newFileIndex,
                                   appendDataIsCompressed,uncompressedSize,compressedSize));
    
    const hpatch_TStreamOutput* outStream=0;
#if (_IS_NEED_VIRTUAL_ZIP)
    switch (ty) {
        case kVirtualZip_out_void: { //copy to zip
            outStream=Zipper_file_append_part_as_stream(self->_out_newZip); } break;
        case kVirtualZip_out_emptyFile_cast: { } break; //nothing out
        case kVirtualZip_out_emptyFile_uncompressed: { //copy to virtualStream
            outStream=self->_vout->virtualStream; } break;
        default: { check(false); } break; //error, or unknow as error
    }
#else //copy to zip
    outStream=Zipper_file_append_part_as_stream(self->_out_newZip);
#endif
    if (outStream){
        if (isNeedDecompress){
            check(UnZipper_fileData_decompressTo(self->_oldZip,oldFileIndex,outStream));
        }else{
            check(UnZipper_fileData_copyTo(self->_oldZip,oldFileIndex,outStream));
        }
    }
    
    check(Zipper_file_append_end(self->_out_newZip));
#if (_IS_NEED_VIRTUAL_ZIP)
    if ((self->_vout)&&(self->_vout->virtualStream))
        check(self->_vout->endVirtual(self->_vout));
#endif
    return true;
}


