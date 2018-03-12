**ApkDiffPatch**
================
Version 0.9.1 alpha   
Zip(Jar,Apk) file by file Diff & Patch C++ library, create minimal differential, support apk v2 sign & Jar sign(apk v1).    
( not support zip64; import lib HDiffPatch, zlib, zstd; )   

---
uses:

*  **ZipDiff(oldZip,newZip,out diffData)**
   
   release the diffData for update oldZip;    
   used apk v2 sign? newZip=AndroidSDK#apksigner(ApkNormalized(newZip)) before ZipDiff.   
   
*  **ZipPatch(oldZip,diffData,out newZip)**
  
   ok , get the newZip; 
   **ZipPatch()** can min requires O(1) bytes of memory when using temp disk file.
   
---
*  **Why need ApkDiffPatch after have BsDiff or HDiffPatch?**  
```
=======================================================================================================
                                    Program                        BsDiff4.3 HDiffPatch  ApkDiffPatch
             oldVersion   size                 newVersion   size    (bzip2)   (+zlib)  (+zlib)  (+zstd)
-------------------------------------------------------------------------------------------------------
chrome-63-0-3239-111.apk 46644851 chrome-64-0-3282-123.apk 43879588 32916357 32621974 16008696 14995721
chrome-64-0-3282-123.apk 43879588 chrome-64-0-3282-137.apk 43879588 28895294 28538751  1497316  1365214
chrome-64-0-3282-137.apk 43879588 chrome-65-0-3325-109.apk 43592997 31771385 31540550 14441986 13381291
google-maps-9-70-0.apk   51304768 google-maps-9-71-0.apk   50568872 37992141 37531799 15019831 13556989
google-maps-9-71-0.apk   50568872 google-maps-9-72-0.apk   54342938 41897706 41475595 19184645 16034933
weixin660android1200.apk 61301280 weixin661android1220.apk 61301316 17565557 17372003  1794310  1717553
weixin661android1220.apk 61301316 weixin662android1240.apk 63595334 38349926 38025483 18408624 17218646
weixin662android1240.apk 63595334 weixin663android1260.apk 63595326 11614415 11531258   937761   938086
weixin663android1260.apk 63595326 weixin665android1280.apk 63669882 21423459 21176790  9031637  8376522
-------------------------------------------------------------------------------------------------------
Average Compression                                         100.0%    56.3%    55.8%    20.6%    18.8%
=======================================================================================================
```
   
---
by housisong@gmail.com  

