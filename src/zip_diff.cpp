//  zip_diff.cpp
//  ZipDiff
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

#include <iostream>
#include <string>
#include "diff/diff.h"
#include "../HDiffPatch/_clock_for_demo.h"

int main(int argc, const char * argv[]) {
    if (argc!=4){
        std::cout << "parameter: oldZip newZip outDiffFileName\n";
        return 1;
    }
    int exitCode=0;
    const char* oldZipPath     =argv[1];
    const char* newZipPath     =argv[2];
    const char* outDiffFileName=argv[3];
    std::string temp_ZipPatchFileName=std::string(outDiffFileName)+"_temp_forTestZipPatch.tmp";
    double time0=clock_s();
    if (!ZipDiff(oldZipPath,newZipPath,outDiffFileName,temp_ZipPatchFileName.c_str())){
        std::cout << "ZipDiff error!\n";
        exitCode=1;
    }
    double time1=clock_s();
    std::cout<<"ZipDiff time:"<<(time1-time0)<<" s\n";
    remove(temp_ZipPatchFileName.c_str());
    return exitCode;
}
