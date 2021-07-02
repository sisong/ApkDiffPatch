//  patch_types.h
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

#ifndef ZipPatch_patch_type_h
#define ZipPatch_patch_type_h
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch_types.h"
#include <stdint.h> //uint32_t uint16_t

#define APKDIFFPATCH_VERSION_MAJOR    1
#define APKDIFFPATCH_VERSION_MINOR    4
#define APKDIFFPATCH_VERSION_RELEASE  0

#define _APKDIFFPATCH_VERSION       APKDIFFPATCH_VERSION_MAJOR.APKDIFFPATCH_VERSION_MINOR.APKDIFFPATCH_VERSION_RELEASE
#define _APKDIFFPATCH_QUOTE(str)    #str
#define _APKDIFFPATCH_EXPAND_AND_QUOTE(str)     _APKDIFFPATCH_QUOTE(str)
#define APKDIFFPATCH_VERSION_STRING _APKDIFFPATCH_EXPAND_AND_QUOTE(_APKDIFFPATCH_VERSION)


#ifndef _IS_NEED_FIXED_ZLIB_VERSION
#   define _IS_NEED_FIXED_ZLIB_VERSION 1  // for compress zip file used fixed zlib version
#endif
#ifndef _IsNeedIncludeDefaultCompressHead
#   define _IsNeedIncludeDefaultCompressHead 0 //for include by self for (de)compressPlugin
#endif

#define _CompressPlugin_zlib   // for decompress zip file

#if (!_IsNeedIncludeDefaultCompressHead)
#   if (_IS_NEED_FIXED_ZLIB_VERSION)
#       include "../../zlib1.2.11/zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#   else
#       include "zlib.h" //default by compiler environment
#   endif
#endif

inline static uint32_t readUInt32(const unsigned char* buf){
    return buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);
}
inline static void writeUInt32_to(unsigned char* out_buf4,uint32_t v){
    out_buf4[0]=(unsigned char)v; out_buf4[1]=(unsigned char)(v>>8);
    out_buf4[2]=(unsigned char)(v>>16); out_buf4[3]=(unsigned char)(v>>24);
}


#endif //ZipPatch_patch_type_h
