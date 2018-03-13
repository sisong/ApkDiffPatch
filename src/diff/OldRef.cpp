//  OldRef.cpp
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
#include "OldRef.h"
#include <algorithm> //sort
#include "DiffData.h"
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff.h"

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        assert(false); return false; } }


inline static void diff(const std::vector<TByte>& newData,
                        const std::vector<TByte>& oldData,std::vector<TByte>& out_diff){
    create_compressed_diff(newData.data(),newData.data()+newData.size(),
                           oldData.data(),oldData.data()+oldData.size(), out_diff);
}

static bool tryRef(std::vector<TByte>& newData,size_t& newDataCompressSize,std::vector<TByte>& curNewData){
    size_t curCompressSize=getNormalizedCompressedSize(curNewData);
    if (curCompressSize<newDataCompressSize){
        newDataCompressSize=curCompressSize;
        newData.swap(curNewData);
        return true;
    }
    return false;
}


bool getOldRefList(UnZipper* newZip,const std::vector<uint32_t>& samePairList,
                   const std::vector<uint32_t>& newRefList,
                   const std::vector<uint32_t>& newRefNotDecompressList,
                   UnZipper* oldZip,std::vector<uint32_t>& out_oldRefList,
                   std::vector<uint32_t>& out_oldRefNotDecompressList){
    std::vector<uint32_t> oldSameList(samePairList.size()/2);
    for (size_t i=0; i<oldSameList.size(); ++i)
        oldSameList[i]=samePairList[i*2+1];
    std::sort(oldSameList.begin(),oldSameList.end());
    
    int oldZipFileCount=UnZipper_fileCount(oldZip);
    std::vector<uint32_t> decompressList;
    size_t iOldSame=0;
    for (int i=0; i<oldZipFileCount; ++i) {
        bool isCompressed=UnZipper_file_isCompressed(oldZip,i);
        bool isInSame=false;
        while ((iOldSame<oldSameList.size())&&(i==oldSameList[iOldSame])){
            isInSame=true;
            ++iOldSame;
        }
        if(!isCompressed)//反正不用解压,不用判断其是否有参考意义了;
            out_oldRefList.push_back(i);
        if (isInSame)
            continue; //跳过和newZip中相同的文件,优化本函数的速度;但某些情况可能会使得结果变大;
        
        if (UnZipper_file_isApkV2Compressed(oldZip,i))
            out_oldRefNotDecompressList.push_back(i);
        else if (isCompressed)
            decompressList.push_back(i);
    }
    assert(iOldSame==oldSameList.size());
    
    //newData-=curOldData;
    std::vector<TByte> newData;
    check(readZipStreamData(newZip,newRefList,newRefNotDecompressList,newData));
    size_t newDataCompressSize=getNormalizedCompressedSize(newData);
    check(newDataCompressSize>0);
    std::vector<TByte> oldData;
    check(readZipStreamData(oldZip,out_oldRefList,out_oldRefNotDecompressList,oldData));
    {
        std::vector<TByte> out_newData;
        diff(newData,oldData,out_newData);
        tryRef(newData,newDataCompressSize,out_newData);
    }
    //从decompressList中删除不相关的文件;
    size_t insert=0;
    for (size_t i=0;i<decompressList.size(); ++i) { //todo:区分顺序?
        oldData.resize(UnZipper_file_uncompressedSize(oldZip,decompressList[i]));
        hpatch_TStreamOutput stream;
        mem_as_hStreamOutput(&stream,oldData.data(),oldData.data()+oldData.size());
        check(UnZipper_fileData_decompressTo(oldZip,decompressList[i],&stream));
        std::vector<TByte> out_newData;
        diff(newData,oldData,out_newData);
        if (tryRef(newData,newDataCompressSize,out_newData))
            decompressList[insert++]=decompressList[i];
    }
    decompressList.resize(insert);
    *
    out_oldRefList.insert(out_oldRefList.end(),decompressList.begin(),decompressList.end());
    std::sort(out_oldRefList.begin(),out_oldRefList.end());
    return true;
}
