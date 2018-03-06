//  DiffData.cpp
//  ZipDiff
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
#include "DiffData.h"
#include <stdio.h>
#include <vector>
#include <map>
#include "../patch/OldStream.h"

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }

static bool isSameFileData(UnZipper* newZip,int newIndex,UnZipper* oldZip,int oldIndex){//safe check
    uint32_t newFileSize=UnZipper_file_uncompressedSize(newZip,newIndex);
    if (newFileSize!=UnZipper_file_uncompressedSize(oldZip,oldIndex)) return false;
    std::vector<TByte> buf(newFileSize*2,0);
    hpatch_TStreamOutput stream;
    mem_as_hStreamOutput(&stream,buf.data(),buf.data()+newFileSize);
    check(UnZipper_fileData_decompressTo(newZip,newIndex,&stream));
    mem_as_hStreamOutput(&stream,buf.data()+newFileSize,buf.data()+newFileSize*2);
    check(UnZipper_fileData_decompressTo(oldZip,oldIndex,&stream));
    return 0==memcmp(buf.data(),buf.data()+newFileSize,newFileSize);
}

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,
                     std::vector<uint32_t>& out_samePairList,
                     std::vector<uint32_t>& out_newRefList,
                     std::vector<uint32_t>& out_newReCompressList){
    int oldFileCount=UnZipper_fileCount(oldZip);
    typedef std::multimap<uint32_t,int> TMap;
    TMap crcMap;
    for (int i=0; i<oldFileCount; ++i) {
        uint32_t crcOld=UnZipper_file_crc32(oldZip,i);
        crcMap.insert(TMap::value_type(crcOld,i));
    }
    
    int newFileCount=UnZipper_fileCount(newZip);
    for (int i=0; i<newFileCount; ++i) {
        uint32_t crcNew=UnZipper_file_crc32(newZip,i);
        std::pair<TMap::const_iterator,TMap::const_iterator> range=crcMap.equal_range(crcNew);
        bool findSame=false;
        for (;range.first!=range.second;++range.first) {
            int oldIndex=range.first->second;
            if (isSameFileData(newZip,i,oldZip,oldIndex)){
                findSame=true;
                out_samePairList.push_back(i);
                out_samePairList.push_back(oldIndex);
                break;
            }
            printf("WARNING: crc32 equal but data not equal! fileIndex(%d,%d)\n",i,oldIndex);
            //todo:print fileName
        }
        if (!findSame){
            out_newRefList.push_back(i);
            if (UnZipper_file_isCompressed(newZip,i))
                out_newReCompressList.push_back(i);
        }
    }
    return true;
}

struct t_auto_OldStream {
    inline t_auto_OldStream(OldStream* stream):_stream(stream){}
    inline ~t_auto_OldStream(){ OldStream_close(_stream); }
    OldStream* _stream;
};

bool readZipStreamData(UnZipper* zip,const std::vector<uint32_t>& refList,std::vector<unsigned char>& out_data){
    long outSize=0;
    OldStream stream;
    OldStream_init(&stream);
    t_auto_OldStream _t_auto_OldStream(&stream);
    
    ZipFilePos_t decompressSumSize=OldStream_getDecompressSumSize(zip,refList.data(),refList.size());
    std::vector<TByte> decompressData(decompressSumSize,0);
    hpatch_TStreamOutput out_decompressStream;
    hpatch_TStreamInput  in_decompressStream;
    mem_as_hStreamOutput(&out_decompressStream,decompressData.data(),decompressData.data()+decompressSumSize);
    mem_as_hStreamInput(&in_decompressStream,decompressData.data(),decompressData.data()+decompressSumSize);
    check(OldStream_getDecompressData(zip,refList.data(),refList.size(),&out_decompressStream));
    check(OldStream_open(&stream,zip,refList.data(),refList.size(),&in_decompressStream));
    
    outSize=stream.stream->streamSize;
    assert(outSize==stream.stream->streamSize);
    out_data.resize(outSize);
    check(outSize==stream.stream->read(stream.stream->streamHandle,0,out_data.data(),out_data.data()+outSize));
    return true;
}

