// apk_patch.cpp
// Created by sisong on 2019-08-22.
#include "apk_patch.h"

TPatchResult ApkPatch(const char *oldApkPath,const char *patchFilePath,const char *outNewApkPath,
                      size_t maxUncompressMemory,const char *tempUncompressFilePath,int threadNum){
    return ZipPatch(oldApkPath,patchFilePath,outNewApkPath,
                    maxUncompressMemory,tempUncompressFilePath,threadNum);
}
