# args
STATIC_CPP := 0
# used clang?
CL  	 := 0
# build with -m32?
M32      := 0
# build for out min size
MINS     := 0

ZLIB_OBJ := \
    zlib1.2.11/adler32.o \
    zlib1.2.11/compress.o \
    zlib1.2.11/crc32.o \
    zlib1.2.11/deflate.o \
    zlib1.2.11/infback.o \
    zlib1.2.11/inffast.o \
    zlib1.2.11/inflate.o \
    zlib1.2.11/inftrees.o \
    zlib1.2.11/trees.o \
    zlib1.2.11/uncompr.o \
    zlib1.2.11/zutil.o

ZIPPATCH_OBJ := \
    src/patch/NewStream.o \
    src/patch/OldStream.o \
    src/patch/Patcher.o \
    src/patch/ZipDiffData.o \
    src/patch/Zipper.o \
    HDiffPatch/libHDiffPatch/HPatch/patch.o \
    HDiffPatch/file_for_patch.o \
    HDiffPatch/libParallel/parallel_import.o \
    HDiffPatch/libParallel/parallel_channel.o \
    lzma/C/LzmaDec.o \
    lzma/C/Lzma2Dec.o \
    $(ZLIB_OBJ)

APKNORM_OBJ := \
    src/apk_normalized.o \
    src/normalized/normalized.o \
    src/diff/DiffData.o \
    $(ZIPPATCH_OBJ)
    
ZIPDIFF_OBJ := \
    src/zip_diff.o \
    src/diff/DiffData.o \
    src/diff/Differ.o \
    src/diff/OldRef.o \
    HDiffPatch/libHDiffPatch/HPatchLite/hpatch_lite.o \
    HDiffPatch/libHDiffPatch/HDiff/diff.o \
    HDiffPatch/libHDiffPatch/HDiff/match_block.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/suffix_string.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/compress_detect.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/limit_mem_diff/digest_matcher.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o \
    lzma/C/Lzma2Enc.o \
    lzma/C/LzmaEnc.o \
    lzma/C/7zStream.o \
    lzma/C/LzFind.o \
    lzma/C/LzFindMt.o \
    lzma/C/LzFindOpt.o \
    lzma/C/CpuArch.o \
    lzma/C/MtCoder.o \
    lzma/C/MtDec.o \
    lzma/C/Threads.o \
    $(ZIPPATCH_OBJ)

DEF_FLAGS := -O3 -DNDEBUG -D_IS_USED_MULTITHREAD=1 -D_IS_USED_PTHREAD=1
CFLAGS   += $(DEF_FLAGS)
CXXFLAGS += $(DEF_FLAGS) -std=c++11

LINK_LIB := -lpthread	# link pthread
ifeq ($(M32),0)
else
  LINK_LIB += -m32
endif
ifeq ($(MINS),0)
else
  LINK_LIB += -Wl,--gc-sections,--as-needed
endif
ifeq ($(CL),1)
  CXX := clang++
  CC  := clang
endif
ifeq ($(STATIC_CPP),0)
  LINK_LIB += -lstdc++
else
  LINK_LIB += -static-libstdc++
endif

.PHONY: all clean

all: ApkNormalized ZipDiff ZipPatch

ApkNormalized: $(APKNORM_OBJ)
	$(CXX) $(APKNORM_OBJ) $(LINK_LIB) -o ApkNormalized
ZipDiff: $(ZIPDIFF_OBJ)
	$(CXX) $(ZIPDIFF_OBJ) $(LINK_LIB) -o ZipDiff
ZipPatch: src/zip_patch.o $(ZIPPATCH_OBJ)
	$(CXX) src/zip_patch.o $(ZIPPATCH_OBJ) $(LINK_LIB) -o ZipPatch

clean:
	-rm -f ApkNormalized ZipDiff ZipPatch src/zip_patch.o $(ZIPDIFF_OBJ) $(APKNORM_OBJ)