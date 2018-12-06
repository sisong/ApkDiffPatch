//  zip_patch.cpp
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
#include <stdio.h>
#include <stdlib.h>
#include "../HDiffPatch/_clock_for_demo.h"
#include "../HDiffPatch/_atosize.h"
#include "patch/Patcher.h"

#ifndef PRSizeT
#   ifdef _MSC_VER
#       define PRSizeT "Iu"
#   else
#       define PRSizeT "zu"
#   endif
#endif

static void printUsage(){
    printf("usage: ZipPatch oldZipFile zipDiffFile outNewZipFile "
           "[maxUncompressMemory tempUncompressFileName]\n"
           "options:\n"
           "  decompress data saved in Memory, when DEFAULT or it's size <= maxUncompressMemory;\n"
           "  if it's size > maxUncompressMemory then save it into file tempUncompressFileName;\n"
           "  maxUncompressMemory can like 8388608 8m 40m 120m 1g etc... \n");
}

int main(int argc, const char * argv[]) {
    const char* oldZipPath=0;
    const char* zipDiffPath=0;
    const char* outNewZipPath=0;
    size_t      maxUncompressMemory=0;
    const char* tempUncompressFileName=0;
    if(argc==4){
        oldZipPath   =argv[1];
        zipDiffPath  =argv[2];
        outNewZipPath=argv[3];
    }else if(argc==6){
        oldZipPath   =argv[1];
        zipDiffPath  =argv[2];
        outNewZipPath=argv[3];
        if (!kmg_to_size(argv[4],strlen(argv[4]),&maxUncompressMemory)){
            printf("argument maxUncompressMemory ERROR!\n");
            printUsage();
            return 1;
        }
        tempUncompressFileName=argv[5];
        if ((tempUncompressFileName==0)||(strlen(tempUncompressFileName)==0)){
            printf("argument tempUncompressFileName ERROR!\n");
            printUsage();
            return 1;
        }
    }else{
        printUsage();
        return 1;
    }
    printf("oldZip   :\"%s\"\nzipDiff  :\"%s\"\noutNewZip:\"%s\"\n",oldZipPath,zipDiffPath,outNewZipPath);
    if (tempUncompressFileName!=0)
        printf("maxUncompressMemory:%" PRSizeT "\ntempUncompressFileName:\"%s\"\n",
               maxUncompressMemory,tempUncompressFileName);
    
    double time0=clock_s();
    int exitCode=ZipPatch(oldZipPath,zipDiffPath,outNewZipPath,
                          maxUncompressMemory,tempUncompressFileName,3);
    double time1=clock_s();
    if (exitCode==PATCH_SUCCESS)
        printf("  zip file patch ok!\n");
    printf("\nZipPatch time: %.3f s\n",(time1-time0));
    return exitCode;
}
