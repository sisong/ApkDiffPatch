package com.github.sisong;

public class ApkPatch{
    public static native int patch(String oldApkPath,String patchFilePath,String outNewApkPath,
                                   long maxUncompressMemory,String tempUncompressFilePath,int threadNum);
}
