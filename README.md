**ApkDiffPatch**
================
[![release](https://img.shields.io/badge/release-v1.0.4-blue.svg)](https://github.com/sisong/ApkDiffPatch/releases)  [![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/ApkDiffPatch/blob/master/LICENSE)  [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/ApkDiffPatch/pulls)   
[![Build Status](https://travis-ci.org/sisong/ApkDiffPatch.svg?branch=master)](https://travis-ci.org/sisong/ApkDiffPatch) [![Build status](https://ci.appveyor.com/api/projects/status/u5tbrqwl72875r6h/branch/master?svg=true)](https://ci.appveyor.com/project/sisong/apkdiffpatch/branch/master)   
a C++ library and command-line tools for Zip(Jar,Apk) file Diff & Patch; create minimal differential; support [Apk v2 sign] & [Jar sign](Apk v1 sign).    
( not support zip64, and only support decode Deflated code; dependent libraries [HDiffPatch], [zlib], [lzma]. )   
[[中文说明](https://blog.csdn.net/housisong/article/details/79768678)]

[HDiffPatch]: https://github.com/sisong/HDiffPatch
[zlib]: https://github.com/madler/zlib
[lzma]: https://www.7-zip.org/sdk.html
[Apk v2 sign]: https://source.android.com/security/apksigning/v2
[Jar sign]: https://docs.oracle.com/javase/8/docs/technotes/guides/jar/jar.html#Signed_JAR_File

---
usage:

*  **ZipDiff(oldZip,newZip,out diffData)**
   
   release the diffData for update oldZip;    
   your apk use v2 sign(or need ZipPatch result byte by byte equal)? `Released newZip` := AndroidSDK#apksigner(**ApkNormalized**(newZip))  before ZipDiff, AND You should not modify the zlib version (unless it is certified compatible).   
   
*  **ZipPatch(oldZip,diffData,out newZip)**
  
   ok , get the newZip;   
   **ZipPatch()** requires 4*(decompress stream memory) + `ref old decompress` memory + O(1), also `ref old decompress` can use temp disk file without memory. 
   
---
*  **Why** need **ApkDiffPatch** after have BsDiff or HDiffPatch **?**
```
ApkDiffPatch: v1.0
BsDiff: v4.3
HDiffPatch: v2.2.6   
Google Play patches: https://github.com/andrewhayden/archive-patcher  v1.0 
                     (test by https://github.com/googlesamples/apk-patch-size-estimator )
=========================================================================================================
                                                           BsDiff HDiffPatch archive-patcher ApkDiffPatch
          oldVersion               newVersion      newSize  (bzip2)  (+zlib)   (+gzip)   (+zlib)  (+lzma)
---------------------------------------------------------------------------------------------------------
chrome-63-0-3239-111.apk chrome-64-0-3282-123.apk 43879588 32916357 32621974  18776996  15995242 13896562
chrome-64-0-3282-123.apk chrome-64-0-3282-137.apk 43879588 28895294 28538751   1357320   1341073  1149331
chrome-64-0-3282-137.apk chrome-65-0-3325-109.apk 43592997 31771385 31540550  16427116  14415021 12356765
google-maps-9-70-0.apk   google-maps-9-71-0.apk   50568872 37992141 37531799  17293163  14562607 11430622
google-maps-9-71-0.apk   google-maps-9-72-0.apk   54342938 41897706 41475595  21301751  18752320 14066134
weixin660android1200.apk weixin661android1220.apk 61301316 17565557 17372003   1927833   1794219  1659286
weixin661android1220.apk weixin662android1240.apk 63595334 38349926 38025483  22100454  18392769 15556315
weixin662android1240.apk weixin663android1260.apk 63595326 11614415 11531258   1044656    937920   940060
weixin663android1260.apk weixin665android1280.apk 63669882 21423459 21176790   9621032   9003472  7392063
---------------------------------------------------------------------------------------------------------
Average Compression                                100.0%    56.3%    55.8%     23.5%     20.4%    16.8%
=========================================================================================================
```
   
---
by housisong@gmail.com  

