# Windbg 通过 Export Table 遍历导出函数
!dh命令列出指定映像的PE Header, 在输出的信息里找到Export Directory的RVA
```
0:000>!dh KERNEL32.DLL
...
    4160  DLL characteristics
            High entropy VA supported
            Dynamic base
            NX compatible
            Guard
   9E700 [    E8F4] address [size] of Export Directory
   ACFF4 [     7F8] address [size] of Import Directory
...
```
EAT(Export Address Table)对应结构为IMAGE_EXPORT_DIRECTORY.   
winnt.h内有关此结构体的代码
```cpp
typedef struct _IMAGE_EXPORT_DIRECTORY {
    DWORD   Characteristics;
    DWORD   TimeDateStamp;
    WORD    MajorVersion;
    WORD    MinorVersion;
    DWORD   Name;
    DWORD   Base;
    DWORD   NumberOfFunctions;
    DWORD   NumberOfNames;
    DWORD   AddressOfFunctions;     // RVA from base of image
    DWORD   AddressOfNames;         // RVA from base of image
    DWORD   AddressOfNameOrdinals;  // RVA from base of image
} IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;
```
通过dd访问映像的EAT结构, 使用dd以每4字节显示内存, 方便访问结构体成员.
```
0:000> dd 9E700+kernel32.dll
00007ffa`240ae700  00000000 8c0b1418 00000000 000a286e
00007ffa`240ae710  00000001 00000687 00000687 0009e728
00007ffa`240ae720  000a0144 000a1b60 000a2893 000a28c9
00007ffa`240ae730  00018c30 00014770 00021420 0005aa80
00007ffa`240ae740  000045e0 00021130 00021140 000a2974
00007ffa`240ae750  0003ccf0 0005aba0 0005ac00 0003ac50
00007ffa`240ae760  00016f80 0003ac70 0001f730 0003ac90
00007ffa`240ae770  000384c0 000a2aad 000a2aed 0004ad40
```
EAT结构内8 * DWORD偏移是AddressOfFunctions的RVA, 此处为0009e728. 此RVA所在地址存储的是导出函数指针表, 表内指针与AddressOfNames内的函数名指针呈映射关系   
9 * DWORD偏移存储AddressOfNames的RVA, 此处值为000a0144. 此RVA所在地址存储导出函数名的指针表, 与AddressOfFunctions呈映射关系.   
导出函数表内条目的总量为0x00000687, 这值位于EAT结构偏移6 * DWORD处.
列出AddressOfFunctions
```
0:000> dd 0009e728+kernel32.dll
00007ffa`240ae728  000a2893 000a28c9 00018c30 00014770
00007ffa`240ae738  00021420 0005aa80 000045e0 00021130
00007ffa`240ae748  00021140 000a2974 0003ccf0 0005aba0
00007ffa`240ae758  0005ac00 0003ac50 00016f80 0003ac70
00007ffa`240ae768  0001f730 0003ac90 000384c0 000a2aad
00007ffa`240ae778  000a2aed 0004ad40 00020d80 0003acd0
00007ffa`240ae788  0003acb0 000a2b80 000a2bbe 000a2c06
00007ffa`240ae798  000a2c59 000a2cb1 000a2d05 000a2d59
```
列出AddressOfNames
```
0:000> dd 000a0144+kernel32.dll
00007ffa`240b0144  000a287b 000a28b4 000a28e7 000a28f6
00007ffa`240b0154  000a290b 000a2930 000a2939 000a2942
00007ffa`240b0164  000a2953 000a2964 000a29a9 000a29cf
00007ffa`240b0174  000a29ee 000a2a0d 000a2a1a 000a2a2d
00007ffa`240b0184  000a2a45 000a2a60 000a2a75 000a2a92
00007ffa`240b0194  000a2ad1 000a2b12 000a2b25 000a2b32
00007ffa`240b01a4  000a2b4c 000a2b6a 000a2ba1 000a2be6
00007ffa`240b01b4  000a2c31 000a2c8c 000a2ce1 000a2d34
```
在AddressOfNames内随机选取一个RVA指针, 此处选取000a2a2d, 处于AddressOfNames偏移16 * DWORD位置. 列出此指针内的函数名.   
```
0:000> da 000a2a2d+kernel32.dll
00007ffa`240b2a2d  "AddResourceAttributeAce"
```
可知其函数名为AddResourceAttributeAce, AddressOfFunctions偏移16 * DWORD位置为RVA指针0003ac70, 列出此位置下指令.
```
0:000> u 0003ac70+kernel32.dll
KERNEL32!AddResourceAttributeAceStub:
00007ffa`2404ac70 48ff25d9ae0400  jmp     qword ptr [KERNEL32!_imp_AddResourceAttributeAce (00007ffa`24095b50)]
00007ffa`2404ac77 cc              int     3
00007ffa`2404ac78 cc              int     3
00007ffa`2404ac79 cc              int     3
```
可见此RVA位置下的指令跳入KERNEL32的_imp_AddResourceAttributeAce函数, 显然此函数体内的指令与前文AddResourceAttributeAce函数名相对应. 证明其确实呈现映射关系.   

