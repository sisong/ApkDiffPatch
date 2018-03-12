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
    $(ZLIB_OBJ)
    
ZIPDIFF_OBJ := \
    src/diff/DiffData.o \
    src/diff/Differ.o \
    src/normalized/normalized.o \
    HDiffPatch/libHDiffPatch/HDiff/diff.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/suffix_string.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/compress_detect.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/limit_mem_diff/digest_matcher.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    HDiffPatch/libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o \
    $(ZIPPATCH_OBJ)

CFLAGS      += -O3
CXXFLAGS   += -O3

.PHONY: all clean

all: ApkNormalized ZipDiff ZipPatch

ApkNormalized: src/apk_normalized.o $(ZIPDIFF_OBJ)
	c++ -o ApkNormalized src/apk_normalized.o $(ZIPDIFF_OBJ)
ZipDiff: src/zip_diff.o $(ZIPDIFF_OBJ)
	c++ -o ZipDiff src/zip_diff.o $(ZIPDIFF_OBJ)
ZipPatch: src/zip_patch.o $(ZIPPATCH_OBJ)
	c++ -o ZipPatch src/zip_patch.o $(ZIPPATCH_OBJ)

clean:
	rm -f ApkNormalized src/apk_normalized.o ZipDiff src/zip_diff.o ZipPatch src/zip_patch.o $(ZIPDIFF_OBJ)
