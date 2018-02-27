//
//  membuf.h
//  ApkPatch
//
//  Created by housisong on 2018/2/26.
//  Copyright © 2018年 housisong. All rights reserved.
//

#ifndef membuf_h
#define membuf_h
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
};

#endif /* membuf_h */
