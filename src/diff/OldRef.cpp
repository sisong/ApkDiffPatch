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

bool getOldRefList(UnZipper* newZip,const std::vector<uint32_t>& samePairList,
                   const std::vector<uint32_t>& newRefList,
                   UnZipper* oldZip,std::vector<uint32_t>& out_oldRefList){
    std::vector<uint32_t> oldSameList(samePairList.size()/2);
    for (size_t i=0; i<oldSameList.size(); ++i)
        oldSameList[i]=samePairList[i*2+1];
    std::sort(oldSameList.begin(),oldSameList.end());
    
    int oldZipFileCount=UnZipper_fileCount(oldZip);
    std::vector<uint32_t> decompressList;
    size_t iOldSame=0;
    for (int i=0; i<oldZipFileCount; ++i) {
        bool isInSame=false;
        while ((iOldSame<oldSameList.size())&&(i==(int)oldSameList[iOldSame])){
            isInSame=true;
            ++iOldSame;
        }
        if (UnZipper_file_uncompressedSize(oldZip,i)<=0)
            continue;
        if(!UnZipper_file_isCompressed(oldZip,i))
            out_oldRefList.push_back(i);//反正不用解压,不用判断其是否有参考意义了;
        else if (!isInSame) //跳过和newZip中相同的文件,但某些情况可能会使得结果变大;
            decompressList.push_back(i);
    }
    assert(iOldSame==oldSameList.size());
    //todo: reduce decompressList;
    out_oldRefList.insert(out_oldRefList.end(),decompressList.begin(),decompressList.end());
    std::sort(out_oldRefList.begin(),out_oldRefList.end());
    return true;
}
