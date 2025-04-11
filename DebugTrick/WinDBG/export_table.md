# Windbg 通过 Export Table 遍历导出函数
!dh命令列出指定映像的PE Header, 在输出的信息里找到Export Directory的RVA
```
0:000>!dh KERNELBASE.DLL
...
    4140  DLL characteristics
            Dynamic base
            NX compatible
            Guard
  23F3D0 [    FA16] address [size] of Export Directory
  254B70 [      3C] address [size] of Import Directory
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
0:000> dd kernelbase+23F3D0
7541f3d0  00000000 8242574f 00000000 002441d2
7541f3e0  00000001 000007c9 000007c9 0023f3f8
7541f3f0  0024131c 00243240 001fd600 001d6890
7541f400  00136dd0 00244237 0013bae0 0014ec00
7541f410  00254b6c 00129a20 0021b6b0 0014e3c0
7541f420  0021b750 0021b800 0021b880 0021b940
```
EAT结构内8 * DWORD偏移是AddressOfFunctions的RVA, 此处为0023f3f8. 此RVA所在地址存储的是导出函数指针表, 表内指针与AddressOfNameOrdinals内的索引呈映射关系   
9 * DWORD偏移存储AddressOfNames的RVA, 此处值为0024131c. 此RVA所在地址存储导出函数名的指针表, 与AddressOfNameOrdinals呈映射关系.   
导出函数表内条目的总量为000007c9, 这值位于EAT结构偏移6 * DWORD处.
列出AddressOfFunctions
```
0:000> dd kernelbase+0023f3f8
7541f3f8  001fd600 001d6890 00136dd0 00244237
7541f408  0013bae0 0014ec00 00254b6c 00129a20
7541f418  0021b6b0 0014e3c0 0021b750 0021b800
7541f428  0021b880 0021b940 00244377 002443ad
7541f438  001f4770 001510a0 0014e030 0021ba00
7541f448  0021ba50 0021baa0 0021bae0 0021bb30
7541f458  00150de0 0021bb80 0021bbc0 0021bc10
```
列出AddressOfNames
```
0:000> dd kernelbase+0024131c
7542131c  0024428f 0024429b 002442b5 002442c7
7542132c  002442e7 00244303 00244335 0024435f
7542133c  00244398 002443cb 002443dc 002443eb
7542134c  002443ff 00244415 0024442f 00244442
7542135c  00244457 00244470 00244477 00244489
7542136c  0024449d 002444b5 002444c6 002444d7
7542137c  002444fa 0024450a 0024451d 0024452d
7542138c  00244542 0024454f 00244567 00244582
```
列出AddressOfNameOrdinals
```
0:000> dw kernelbase+00243240
75423240  0007 0008 0009 000a 000b 000c 000d 000e
75423250  000f 0010 0011 0012 0013 0014 0015 0016
75423260  0017 0018 0019 001a 001b 001c 001d 001e
75423270  001f 0020 0021 0022 0023 0024 0025 0026
75423280  0027 0028 0029 002a 002b 002c 002d 002f
75423290  002e 0030 0031 0032 0033 0034 0035 0036
754232a0  0037 0038 0039 003a 003b 003c 003d 003e
754232b0  003f 0040 0041 0042 0043 0044 0045 0046
```
在AddressOfNames内随机选取一个RVA指针, 此处选取0x416 * DWORD偏移处的指针, 列出此指针内的函数名.   
```
0:000> dd kernelbase+0024131c+(0x416*4)
75422374  0024a124 0024a131 0024a140 0024a14f
...
0:000> da kernelbase+0024a124
7542a124  "LoadLibraryA"

```
可知其函数名为LoadLibraryA, AddressOfNameOrdinals偏移0x416 * WORD位置的索引为0x418, 根据此索引列出AddressOfFunctions中0x418 * DWORD位置的RVA指针.   
可见其值为001510e0, 而根据此RVA反汇编所得到的偏移位置, 显然在LoadLibraryA接口的方法体内.
```
0:000> dw kernelbase+00243240+(2*0x416)
75423a6c  0418 0419 041a 041b 041c 041d 041e 041f
...
0:000> dd kernelbase+0023f3f8+(4*0x418)
75420458  001510e0 0012aec0 0012bec0 00151b10
...
0:000> u kernelbase+001510e0
KERNELBASE!LoadLibraryA:
753310e0 8bff            mov     edi,edi
753310e2 55              push    ebp
753310e3 8bec            mov     ebp,esp
753310e5 51              push    ecx
753310e6 837d0800        cmp     dword ptr [ebp+8],0
753310ea 53              push    ebx
753310eb 56              push    esi
753310ec 7418            je      KERNELBASE!LoadLibraryA+0x26 (75331106)
```
