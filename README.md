# CSAPP-Labs
這個 Repo 為 CSAPP/3e ICS Lab 的實作和筆記。整本讀完之後，確實對軟體系統層面有全面性的了解，若對軟體系統行為有興趣的話，買書閱讀並動手實作吧!

## 實驗筆記
1. [Data Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/Sk9_ygs6_)   
    訓練學員對於位元操作、整數和浮點數的相關概念
2. [Bomb Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/HJt0Tvr1K)   
    必需透過 gdb 等除錯工具，進行反編譯及逆向工程，找出正確的六個字串拆除炸彈
3. [Attack Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/H1Z8o7QlK)   
    - 學習到針對 buffer overflow 的不同攻擊方法
    - 了解如何寫出更安全的程式，以及編譯器及作業系統如何提供額外的保護機制
    - 了解 x86-64 的 stack 以及參數傳入機制
    - 了解 x86-64 的指令如何編碼
    - 熟練 GDB & OBJDUMP 等除錯工具
4. [Cache Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/rkRCq_vbY)    
  讓學生了解快取的機制及程式優化等議題。實驗分為兩個部份   
  Part A: 寫一個小的 C 程式 (200~300 行)，以 LRU 策略模擬快取的行為，利用 valgrind 的 trace 檔計算 cache hit, miss & eviction   
  Part B: 優化矩陣轉置，盡可能降低 cache miss 的發生次數
5. [Shell Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/SydD27RzY)    
  Shell Lab 對應第八章『異常控制流』，實作一個簡單的 shell tsh
6. [Malloc Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/r13xrF0Vt)    
    本次實驗要求學生實作動態記憶體分配器，包含 malloc、free 及 realloc
7. [Proxy Lab 筆記](https://hackmd.io/@Chang-Chia-Chi/H1E3L6oOt)   
    實作一個可並行處理的 proxy，讓學員學習到 web 的 client-server 機制及多執行緒程式撰寫   
