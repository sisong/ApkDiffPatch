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
    HDiffPatch/libHDiffPatch/HDiff/diff.o \
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
    lzma/C/LzFind.o \
    lzma/C/LzFindMt.o \
    lzma/C/MtCoder.o \
    lzma/C/MtDec.o \
    lzma/C/Threads.o \
    $(ZIPPATCH_OBJ)

CFLAGS      += -O2 -DNDEBUG -D_IS_USED_PTHREAD=1
CXXFLAGS   += -O2 -DNDEBUG -D_IS_USED_PTHREAD=1

.PHONY: all clean

all: ApkNormalized ZipDiff ZipPatch

ApkNormalized: $(APKNORM_OBJ)
	$(CXX) $(APKNORM_OBJ) -lpthread -o ApkNormalized
ZipDiff: $(ZIPDIFF_OBJ)
	$(CXX) $(ZIPDIFF_OBJ) -lpthread -o ZipDiff
ZipPatch: src/zip_patch.o $(ZIPPATCH_OBJ)
	$(CXX) src/zip_patch.o $(ZIPPATCH_OBJ) -lpthread -o ZipPatch

clean:
	-rm -f ApkNormalized ZipDiff ZipPatch src/zip_patch.o $(ZIPDIFF_OBJ) $(APKNORM_OBJ)