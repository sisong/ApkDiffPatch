//  membuf.h
//  ZipPatch
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

#ifndef ZipPatch_membuf_h
#define ZipPatch_membuf_h
#include <assert.h>
#include <string.h> //malloc,free

class MemBuf {
public:
    inline MemBuf():_buf(0),_bufCap(0){}
    inline ~MemBuf(){ clear(); }
    inline unsigned char* buf(){ return _buf; }
    inline size_t cap()const{ return _bufCap; }
    inline void clear(){ if (_buf) { free(_buf); _buf=0; _bufCap=0; } }
    inline void setNeedCap(size_t needCap){ if (needCap<=_bufCap) ; else newMem(needCap); }
private:
    unsigned char*  _buf;
    size_t          _bufCap;
    void newMem(size_t needCap){
        clear();
        _bufCap=needCap;
        _buf=(unsigned char*)malloc(needCap);
        assert(_buf!=0);
    }
};

class MemBufWriter{
private:
    unsigned char*  _buf;
    size_t          _bufCap;
    size_t          _curLen;
public:
    inline MemBufWriter(unsigned char*  buf,size_t bufCap)
    :_buf(buf),_bufCap(bufCap),_curLen(0){}
    inline size_t len()const{ return _curLen; }
    inline bool write(const unsigned char*data,size_t dataLen){
        if (_curLen+dataLen<=_bufCap){
            memcpy(_buf+_curLen,data,dataLen);
            _curLen+=dataLen;
            return true;
        }else{
            return false;
        }
    }
    
    static bool writer(void* dstHandle,const unsigned char* data,const  unsigned char* dataEnd){
        MemBufWriter* writer=(MemBufWriter*)dstHandle;
        return writer->write(data,dataEnd-data);
    }
};

#endif //ZipPatch_membuf_h
