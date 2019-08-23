// apk_patch.h
// Created by sisong on 2019-08-22.
#ifndef apk_patch_h
#define apk_patch_h
#include "../../src/patch/Patcher.h"
#ifdef __cplusplus
extern "C" {
#endif
    #define APK_PATCH_EXPORT __attribute__((visibility("default")))
    //same as ZipPatch in Patcher.h
    TPatchResult ApkPatch(const char *oldApkPath,const char *patchFilePath,const char *outNewApkPath,
                          size_t maxUncompressMemory,const char *tempUncompressFilePath,
                          int threadNum) APK_PATCH_EXPORT;

#ifdef __cplusplus
}
#endif
#endif // apk_patch_h
