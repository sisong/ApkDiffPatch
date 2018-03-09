**ApkDiffPatch**
================
Version 0.9 alpha   
Zip(Jar,Apk) file by file Diff & Patch C++ library, create minimal differential, support Jar signing(apk v1) and apk v2 signing.    
(used HDiffPatch, zlib, zstd)   
---
uses:

*  **ZipDiff(oldZip,newZip,out diffData)**
   
   release the diffData for update oldZip;    
   used apk v2 signed? newZip=AndroidSDK#apksigner(ApkNormalized(newZip)) before ZipDiff.   
   
*  **ZipPatch(oldZip,diffData,out newZip)**
  
   ok , get the newZip; 
   **ZipPatch()** min requires O(1) bytes of memory when using temp disk file.
   
---
by housisong@gmail.com  

