//  zip_patch.cpp
//  ZipPatch
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong
 
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
#include <stdio.h>
#include "../HDiffPatch/_clock_for_demo.h"
#include "src/patch.h"

int main(int argc, const char * argv[]) {
    const char* oldZipFile=0;
    const char* zipDiffFile=0;
    const char* outNewZipFile=0;
    size_t maxUncompressMemory=0;
    const char* tempUncompressFileName=0;
    if (argc==6){
        oldZipFile   =argv[1];
        zipDiffFile  =argv[2];
        outNewZipFile=argv[3];
        long _byteSize=atol(argv[4]);
        if (_byteSize<0){
            printf("parameter maxUncompressMemory error!\n");
            return 1;
        }
        maxUncompressMemory=(size_t)_byteSize;
        tempUncompressFileName=argv[5];
    }else if (argc==4){
        oldZipFile   =argv[1];
        zipDiffFile  =argv[2];
        outNewZipFile=argv[3];
    }else{
        printf("parameter: oldZip zipDiffFile outNewZip [maxUncompressMemory tempUncompressFileName]\n");
        return 1;
    }
    
    int exitCode=0;
    double time0=clock_s();
    if (!(ZipPatch(oldZipFile,zipDiffFile,outNewZipFile,maxUncompressMemory,tempUncompressFileName)))
        exitCode=1;
    double time1=clock_s();
    if (exitCode==0)
        printf("  patch ok!\n");
    printf("\nZipPatch time: %.3f s\n",(time1-time0));
    return exitCode;
}
