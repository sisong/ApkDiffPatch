//  normalized.cpp
//  ApkNormalized
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
#include "normalized.h"
#include <vector>
#include <string>
#include <algorithm> //sort
#include "../patch/Zipper.h"
#include "../diff/DiffData.h"

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }

struct TFileValue{
    std::string fileName;
    int         fileIndex;
    static bool inline isInSignDir(const std::string& s){
        return (s.find("META-INF/")==0)||(s.find("META-INF\\")==0);
    }
    static bool inline isDirTag(char c){
        return (c=='\\')|(c=='/');
    }
    static bool inline isEndWith(const std::string& s,const char* sub){
        return (s.find(sub)==s.size()-strlen(sub));
    }

    static bool inline isSignMFFile(const std::string& s){
        return (s=="META-INF/MANIFEST.MF")||(s=="META-INF\\MANIFEST.MF");
    }
    struct TCmp{
        inline explicit TCmp(int fileCount):_fileCount(fileCount){}
        size_t _v(const TFileValue& x)const{
            size_t xi=x.fileIndex;
            if (isInSignDir(x.fileName)){
                xi+=_fileCount;
                     if (isEndWith(x.fileName,".SF"))  xi+=_fileCount;
                else if (isEndWith(x.fileName,".RSA")) xi+=_fileCount*2;
                else if (isSignMFFile(x.fileName))     xi+=_fileCount*3;
            }
            return  xi;
        }
        inline bool operator ()(const TFileValue& x,const TFileValue& y)const{
            return _v(x)<_v(y);
        }
        int _fileCount;
    };
    struct TCmpByName{
        inline bool operator ()(const TFileValue* x,const TFileValue* y)const{
            return x->fileName<y->fileName;
        }
    };
};

static void getAllFiles(const UnZipper* zip,std::vector<TFileValue>& out_files){
    int fileCount=UnZipper_fileCount(zip);
    out_files.resize(fileCount);
    for (int i=0; i<fileCount; ++i) {
        TFileValue& fi=out_files[i];
        fi.fileIndex=i;
        fi.fileName.assign(zipFile_name(zip,i));
    }
}

static int removeNonEmptyDirs(const UnZipper* zip,std::vector<TFileValue>& files){
    int fileCount=UnZipper_fileCount(zip);
    {
        std::vector<TFileValue*> rfiles(fileCount);
        for (int i=0; i<fileCount; ++i)
            rfiles[i]=&files[i];
        std::sort(rfiles.begin(),rfiles.end(),TFileValue::TCmpByName());
        for (int i=0;i<fileCount-1;++i) {
            if (UnZipper_file_uncompressedSize(zip,rfiles[i]->fileIndex)>0) continue;
            const std::string& nx=rfiles[i]->fileName;
            const std::string& ny=rfiles[i+1]->fileName;
            const size_t nL=nx.size();
            if (nL>ny.size()) continue;
            if (0!=memcmp(nx.c_str(),ny.c_str(),nL)) continue;
            if (((nL>0)&&TFileValue::isDirTag(nx[nL-1]))||
              ((nL<ny.size())&&TFileValue::isDirTag(ny[nL])))
                rfiles[i]->fileIndex=-1; //need remove
        }
    }
    int insert=0;
    for (int i=0; i<fileCount; ++i){
        if (files[i].fileIndex<0) continue;
        if (i!=insert){
            files[insert].fileIndex=files[i].fileIndex;
            files[insert].fileName.swap(files[i].fileName);
        }
        ++insert;
    }
    files.resize(insert);
    return fileCount-insert;
}
inline static bool isCompressedEmptyFile(const UnZipper* unzipper,int fileIndex) {
    return (0==UnZipper_file_uncompressedSize(unzipper,fileIndex))
            &&UnZipper_file_isCompressed(unzipper,fileIndex);
}

bool ZipNormalized(const char* srcApk,const char* dstApk,int ZipAlignSize,int compressLevel,
                   bool isNotCompressEmptyFile,bool isPageAlignSoFile,int* out_apkFilesRemoved){
    bool result=true;
    bool _isInClear=false;
    int  fileCount=0;
    bool isHaveApkV2Sign=false;
    bool isHaveApkV3Sign=false;
    int  jarSignFileCount=0;
    std::vector<int>   fileIndexs;
    std::vector<std::string>  removedFiles;
    std::vector<std::string> _compressedEmptyFiles; //only for WARNING
    UnZipper unzipper;
    Zipper   zipper;
    UnZipper_init(&unzipper);
    Zipper_init(&zipper);
    
    check(UnZipper_openFile(&unzipper,srcApk));
    fileCount=UnZipper_fileCount(&unzipper);
    check(Zipper_openFile(&zipper,dstApk,fileCount,ZipAlignSize,isPageAlignSoFile,
                          compressLevel,kDefaultZlibCompressMemLevel));
    isHaveApkV2Sign=UnZipper_isHaveApkV2Sign(&unzipper);
    isHaveApkV3Sign=UnZipper_isHaveApkV3Sign(&unzipper);
    {
        int apkFilesRemoved=0;
        std::vector<TFileValue> files;
        getAllFiles(&unzipper,files);
        apkFilesRemoved=removeNonEmptyDirs(&unzipper,files);
        fileCount=(int)files.size();
        std::sort(files.begin(),files.end(),TFileValue::TCmp(UnZipper_fileCount(&unzipper)));
        for (int i=0; i<fileCount; ++i) {
            int fileIndex=files[i].fileIndex;
            if (UnZipper_file_isApkV1Sign(&unzipper,fileIndex)){
                ++jarSignFileCount;
                if (isHaveApkV2Sign){
                    ++apkFilesRemoved;
                    removedFiles.push_back(files[i].fileName);
                    continue; //remove JarSign(ApkV1Sign) when found ApkV2Sign
                }
            }
            if (UnZipper_file_isStampCertFile(&unzipper,fileIndex)){
                ++apkFilesRemoved;
                removedFiles.push_back(files[i].fileName);
                continue; //remove stamp cert file
            }

            fileIndexs.push_back(fileIndex);
        }
        if (out_apkFilesRemoved)
            *out_apkFilesRemoved=apkFilesRemoved;
    }
    
    printf("\n");
    for (int i=0; i<(int)fileIndexs.size(); ++i) {
        int fileIndex=fileIndexs[i];
        std::string fileName=zipFile_name(&unzipper,fileIndex);
        printf("\"%s\"\n",fileName.c_str());
        if (compressLevel==0){
            check(Zipper_file_append_set_new_isCompress(&zipper,false));
        } else if (isCompressedEmptyFile(&unzipper,fileIndex)){
            if (isNotCompressEmptyFile){
                check(Zipper_file_append_set_new_isCompress(&zipper,false));
                printf("NOTE: \"%s\" is a compressed empty file, change to uncompressed!\n",fileName.c_str());
            }else{
                _compressedEmptyFiles.push_back(fileName);
             }
        }
        bool isAlwaysReCompress=true;
        check(Zipper_file_append_copy(&zipper,&unzipper,fileIndex,isAlwaysReCompress));
    }
    printf("\n");

    //no run: check(Zipper_copyExtra_before_fileHeader(&zipper,&unzipper));
    for (int i=0; i<(int)fileIndexs.size(); ++i) {
        int fileIndex=fileIndexs[i];
        if (compressLevel==0){
            check(Zipper_file_append_set_new_isCompress(&zipper,false));
        } else if (isNotCompressEmptyFile&&isCompressedEmptyFile(&unzipper,fileIndex)){
            check(Zipper_fileHeader_append_set_new_isCompress(&zipper,false));
        }
        check(Zipper_fileHeader_append(&zipper,&unzipper,fileIndex));
    }
    check(Zipper_endCentralDirectory_append(&zipper,&unzipper));
    
    for (int i=0;i<(int)_compressedEmptyFiles.size();++i){
        printf("WARNING: \"%s\" is a compressed empty file, can't patch by old(version<v1.3.5) ZipPatch!)\n",_compressedEmptyFiles[i].c_str());
    }
    if (jarSignFileCount>0){
        if (isHaveApkV2Sign){
            printf("WARNING: src removed JarSign(ApkV1Sign) (%d file, need re-sign)\n",jarSignFileCount);
        }else{
            printf("WARNING: src JarSign(ApkV1Sign) (%d file) has been Normalized, Don't re-sign!\n",jarSignFileCount);
        }
    }
    for (size_t i=0;i<removedFiles.size();++i)
        printf("WARNING:   removed file: %s\n",removedFiles[i].c_str());
    if (isHaveApkV2Sign){
        printf(isHaveApkV3Sign?
                "WARNING: src removed ApkV2Sign & ApkV3Sign  data (%d Byte, need re-sign)\n"
               :"WARNING: src removed ApkV2Sign data (%d Byte, need re-sign)\n",
                (int)UnZipper_ApkV2SignSize(&unzipper));
    }
    if (zipper._normalizeSoPageAlignCount>0)
        printf("WARNING: found uncompressed .so file & do page-align, it can't patch by old(version<v1.7.0) ZipPatch!\n");
    printf("src fileCount:%d\nout fileCount:%d\n\n",fileCount,(int)fileIndexs.size());

clear:
    _isInClear=true;
    check(UnZipper_close(&unzipper));
    check(Zipper_close(&zipper));
    return result;
}
