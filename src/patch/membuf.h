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

#endif /* membuf_h */
