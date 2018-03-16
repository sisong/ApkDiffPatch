//  zip_diff.cpp
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
#include <iostream>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diff/Differ.h"
#include "../HDiffPatch/_clock_for_demo.h"

int main(int argc, const char * argv[]) {
    if ((argc<4)||(argc>6)){
        std::cout << "parameter: oldZip newZip outDiffFileName [isEnableEditApkV2Sign(0/1) temp_forTestZipPatchFileName]\n";
        return 1;
    }
    int exitCode=0;
    const char* oldZipPath     =argv[1];
    const char* newZipPath     =argv[2];
    const char* outDiffFileName=argv[3];
    bool isEnableEditApkV2Sign=false;
    std::string temp_ZipPatchFileName=std::string(outDiffFileName)+"_temp_forTestZipPatch.tmp";
    if (argc>=5){
        const char* strBool=argv[4];
        isEnableEditApkV2Sign = (strlen(strBool)==1) && ('1'==strBool[0]);
    }
    if (argc>=6){
        temp_ZipPatchFileName=argv[5];
        assert(!temp_ZipPatchFileName.empty());
    }
    
    std::cout<<"oldZip :\"" <<oldZipPath<< "\"\nnewZip :\""<<newZipPath<<"\"\noutDiff:\""<<outDiffFileName<<"\"\n";
    if (isEnableEditApkV2Sign)
        std::cout<<"  NOTE: isEnableEditApkV2Sign ON!\n";
    double time0=clock_s();
    if (!ZipDiff(oldZipPath,newZipPath,outDiffFileName,temp_ZipPatchFileName.c_str(),isEnableEditApkV2Sign)){
        std::cout << "ZipDiff error!\n";
        exitCode=1;
    }
    double time1=clock_s();
    std::cout<<"\nZipDiff all time:"<<(time1-time0)<<" s\n";
    remove(temp_ZipPatchFileName.c_str());
    return exitCode;
}
