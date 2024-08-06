# args
STATIC_CPP := 0
STATIC_C := 0
# used clang?
CL  	 := 0
# build with -m32?
M32      := 0
# build for out min size
MINS     := 0

ZLIB_OBJ := \
    zlib1.3.1/adler32.o \
    zlib1.3.1/compress.o \
    zlib1.3.1/crc32.o \
    zlib1.3.1/deflate.o \
    zlib1.3.1/infback.o \
    zlib1.3.1/inffast.o \
    zlib1.3.1/inflate.o \
    zlib1.3.1/inftrees.o \
    zlib1.3.1/trees.o \
    zlib1.3.1/uncompr.o \
    zlib1.3.1/zutil.o

ZIPPATCH_OBJ := \
    src/patch/OldStream.o \
    src/patch/ZipDiffData.o \
    src/patch/Zipper.o \
    HDiffPatch/libHDiffPatch/HPatch/patch.o \
    HDiffPatch/file_for_patch.o \
    HDiffPatch/libParallel/parallel_import.o \
    HDiffPatch/libParallel/parallel_channel.o \
    $(ZLIB_OBJ)

APKNORM_OBJ := \
    src/apk_normalized.o \
    src/normalized/normalized.o \
    src/diff/DiffData.o \
    $(ZIPPATCH_OBJ)
    
ZIPPATCH_OBJ += \
    src/patch/NewStream.o \
    src/patch/Patcher.o \
    lzma/C/LzmaDec.o \
    lzma/C/Lzma2Dec.o

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

ZIPPATCH_OBJ += src/zip_patch.o

DEF_FLAGS := -O3 -DNDEBUG -D_IS_USED_MULTITHREAD=1 -D_IS_USED_CPP11THREAD=1
ifeq ($(MINS),0)
else
  DEF_FLAGS += \
    -s \
    -Wno-error=format-security \
    -fvisibility=hidden  \
    -ffunction-sections -fdata-sections \
    -ffat-lto-objects -flto
  CXXFLAGS += -fvisibility-inlines-hidden
endif
CFLAGS   += $(DEF_FLAGS)
CXXFLAGS += $(DEF_FLAGS) -std=c++11

DEF_LINK := -lpthread	# link pthread
ifeq ($(M32),0)
else
  DEF_LINK += -m32
endif
ifeq ($(MINS),0)
else
  DEF_LINK += -s -Wl,--gc-sections,--as-needed
endif
ifeq ($(STATIC_CPP),0)
  DEF_LINK += -lstdc++
else
  DEF_LINK += -static-libstdc++
endif
ifeq ($(STATIC_C),0)
else
  DEF_LINK += -static
endif
ifeq ($(CL),1)
  CXX := clang++
  CC  := clang
endif

.PHONY: all clean

all: ApkNormalized ZipDiff ZipPatch

ApkNormalized: $(APKNORM_OBJ)
	$(CXX) $(APKNORM_OBJ) $(DEF_LINK) -o ApkNormalized
ZipDiff: $(ZIPDIFF_OBJ)
	$(CXX) $(ZIPDIFF_OBJ) $(DEF_LINK) -o ZipDiff
ZipPatch: $(ZIPPATCH_OBJ)
	$(CXX) $(ZIPPATCH_OBJ) $(DEF_LINK) -o ZipPatch

clean:
	-rm -f ApkNormalized ZipDiff ZipPatch  $(ZIPDIFF_OBJ)  $(ZIPPATCH_OBJ) $(APKNORM_OBJ)