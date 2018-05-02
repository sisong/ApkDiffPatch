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
    lzma/C/LzmaDec.o \
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
    lzma/C/LzmaEnc.o \
    lzma/C/LzFind.o \
    $(ZIPPATCH_OBJ)

CFLAGS      += -O3 -D_7ZIP_ST
CXXFLAGS   += -O3

.PHONY: all clean

all: ApkNormalized ZipDiff ZipPatch

ApkNormalized: $(APKNORM_OBJ)
	c++ -o ApkNormalized $(APKNORM_OBJ)
ZipDiff: $(ZIPDIFF_OBJ)
	c++ -o ZipDiff $(ZIPDIFF_OBJ)
ZipPatch:  src/zip_patch.o $(ZIPPATCH_OBJ)
	c++ -o ZipPatch src/zip_patch.o $(ZIPPATCH_OBJ)

clean:
	-rm -f ApkNormalized ZipDiff ZipPatch src/zip_patch.o $(ZIPDIFF_OBJ) $(APKNORM_OBJ)