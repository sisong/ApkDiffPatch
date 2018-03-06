//  DiffData.h
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
#ifndef ZipDiff_DiffData_h
#define ZipDiff_DiffData_h
#include <vector>
#include "../patch/Zipper.h"
#include "../patch/ZipDiffData.h"

bool getSamePairList(UnZipper* newZip,UnZipper* oldZip,
                     std::vector<uint32_t>& out_samePairList);
void getNewRefList(int newZip_fileCount,const std::vector<uint32_t>& samePairList,
                   std::vector<uint32_t>& out_newRefList);

bool readZipStreamData(UnZipper* zip,const std::vector<uint32_t>& refList,
                       std::vector<unsigned char>& out_data);

#endif //ZipDiff_DiffData_h
