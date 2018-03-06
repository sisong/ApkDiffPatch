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
#include <map>

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

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,std::vector<uint32_t>& out_samePairList){
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
        for (;range.first!=range.second;++range.first) {
            int oldIndex=range.first->second;
            if (isSameFileData(newZip,i,oldZip,oldIndex)){
                out_samePairList.push_back(i);
                out_samePairList.push_back(oldIndex);
                break;
            }
        }
    }
    return true;
}

void getNewRefList(int newZip_fileCount,const std::vector<uint32_t>& samePairList,
                   std::vector<uint32_t>& out_newRefList){
    
    //todo:
    return;
}


bool readZipStreamData(UnZipper* zip,const std::vector<uint32_t>& refList,std::vector<unsigned char>& out_data){
    
    //todo:
    return false;
}

