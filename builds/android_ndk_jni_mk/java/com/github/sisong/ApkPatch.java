package com.github.sisong;

public class ApkPatch{
    // return TPatchResult, 0 is ok; patchFilePath file created by ZipDiff;
    public static native int patch(String oldApkPath,String patchFilePath,String outNewApkPath,
                                   long maxUncompressMemory,String tempUncompressFilePath,int threadNum);
}
