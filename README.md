**ApkDiffPatch**
================
Version 1.0   
Zip(Jar,Apk) file by file Diff & Patch C++ library, create minimal differential, support apk v2 sign & Jar sign(apk v1).    
( not support zip64, and only support decode Deflated code; dependent libraries HDiffPatch, zlib, lzma. )   

---
uses:

*  **ZipDiff(oldZip,newZip,out diffData)**
   
   release the diffData for update oldZip;    
   your apk use v2 sign? newZip=AndroidSDK#apksigner(**ApkNormalized**(newZip)) before ZipDiff, AND you cannot change zlib version because v2 Apk need byte by byte equal.   
   
*  **ZipPatch(oldZip,diffData,out newZip)**
  
   ok , get the newZip; 
   **ZipPatch()** can min requires O(1) bytes of memory when using temp disk file.
   
---
*  **Why** need **ApkDiffPatch** after have BsDiff or HDiffPatch **?**
```
ApkDiffPatch: v1.0  BsDiff: v4.3  HDiffPatch: v2.2.6
=======================================================================================================
                                                                    BsDiff  HDiffPatch  ApkDiffPatch
             oldVersion   size                 newVersion   size    (bzip2)   (+zlib)  (+zlib)  (+lzma)
-------------------------------------------------------------------------------------------------------
chrome-63-0-3239-111.apk 46644851 chrome-64-0-3282-123.apk 43879588 32916357 32621974 15995242 13896562
chrome-64-0-3282-123.apk 43879588 chrome-64-0-3282-137.apk 43879588 28895294 28538751  1341073  1149331
chrome-64-0-3282-137.apk 43879588 chrome-65-0-3325-109.apk 43592997 31771385 31540550 14415021 12356765
google-maps-9-70-0.apk   51304768 google-maps-9-71-0.apk   50568872 37992141 37531799 14562607 11430622
google-maps-9-71-0.apk   50568872 google-maps-9-72-0.apk   54342938 41897706 41475595 18752320 14066134
weixin660android1200.apk 61301280 weixin661android1220.apk 61301316 17565557 17372003  1794219  1659286
weixin661android1220.apk 61301316 weixin662android1240.apk 63595334 38349926 38025483 18392769 15556315
weixin662android1240.apk 63595334 weixin663android1260.apk 63595326 11614415 11531258   937920   940060
weixin663android1260.apk 63595326 weixin665android1280.apk 63669882 21423459 21176790  9003472  7392063
-------------------------------------------------------------------------------------------------------
Average Compression                                         100.0%    56.3%    55.8%    20.4%    16.8%
=======================================================================================================
```
   
---
by housisong@gmail.com  

