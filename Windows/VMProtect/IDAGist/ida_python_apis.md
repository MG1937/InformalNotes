# API Note

## 需求描述
https://en.wikipedia.org/wiki/Optimizing_compiler  
将编译器优化技术运用在VMP分析上，拟进行：  
1. SSA  
2. 死存储消除  
3. 活跃变量分析  
4. 控制流分析  
5. 死代码消除  

进行这些工作的必要前提：  
1. 能够识别基本控制流，获取基本块的前驱与后继。  
2. 需要能够识别指令类型，指令功能，对寄存器的影响(def-use判断)。
3. 需要对指令进行修补。  

## 模块基础概念
来源：https://docs.hex-rays.com/9.0sp1/developer-guide/idapython/idapython-getting-started#basics  
idautils: This module extracts the most useful and handy functions allowing you to jump right away and interact with the disassembly without the need to scrutinize the whole IDAPython API from the start. You can find here functions to iterate through whole segments, functions (founded by IDA and also user-defined), named locations, etc.  
idautils: 此模块包含了一批常用的api，并高度封装了许多操作，允许开发者快速地与反汇编器进行一些有效交互，比如操作或寻找函数（无论是IDA定义还是用户自定义），遍历整个节/段/被命名的地址等等，大部分功能在idautils内都可以找得到，而无需再耗费大量精力去检索其它模块的api。  

ida_idaapi: The ida_idaapi module comes handy when you want to create custom plugins or handle events, as it gives you access to more advanced functions and allows interaction with overall system  
ida_idaapi: 给插件开发人员准备的模块，目前无需关注  

idc: This module provides functions that were originally part of native IDA IDC scripting language, wrapped for use in IDAPython API. This is a great starting point if you're already familiar with IDC scripting.  
idc: 该模块包含了许多IDA原生或者IDC脚本支持的操作，功能更加强大，指令级别的细粒度操作通常需要idc模块支持。  

ida_funcs: This module gives you tools to create and manipulate functions within IDA.  
ida_funcs: 该模块用来创建或者操作函数块  

ida_kernwin: This module provides functionality to interact with IDA UI, so you can create custom dialogs and more.  
ida_kernwin: 用来与IDA UI进行交互  

## 样例收集
收集可能用到的IDA Python SDK/API样例。  

### API List
- IDA Python API Cheatsheet  
https://gist.github.com/icecr4ck/7a7af3277787c794c66965517199fc9c  

- 获取光标地址  
idc.get_screen_ea()  
idc.here()  

- 获取某地址下的汇编  
idc.GetDisasm(ea) 直接返回该地址下汇编的字符串格式  

- 地址转指令对象  
idautils.DecodeInstruction(ea) 返回[ida_ua.insn_t](https://python.docs.hex-rays.com/ida_ua/index.html#ida_ua.insn_t)对象，其操作细粒度更高，更加结构化。此结构内的部分关键字段如下：
    - ea：指令所在地址  
    - itype：指令操作码类型，对应的类型描述在ida_allins模块下或者[IDA SDK](https://github.com/HexRaysSA/ida-sdk)的ins.cpp内可以找到。在一例增强反编译器处理SVC指令的样例代码[vds8.py](https://github.com/HexRaysSA/IDAPython/blob/9.0sp1/examples/decompiler/vds8.py)中，提及了itype以及ida_allins的用法。  
    - size：指令长度  
    - ops：操作数数组（op_t结构数组）  

- 操作数对象  
ida_ua.op_t 该对象结构内同样有一些需要关注的关键字段：  
    - type：操作数类型optype_t，由ida_ua.o_前缀的字段描述，例如o_void，o_reg等等  
    - reg：操作数具体的寄存器类型（如果是寄存器的话），可以通过ida_idp.ph_get_regnames()获取当前处理器支持的寄存器列表；通过ida_idp.get_reg_name接口可以透过类型获取寄存器具体值。  
    - dtype：操作数值的数据类型op_dtype_t，由ida_ua.dt_前缀的字段描述，例如dt_byte，dt_word等等。不过需要注意的是，这些类型的值不直接代表该数据类型的长度，获取类型长度需要通过ida_ua.get_dtype_size接口进行。  
获取操作数寄存器具体值的示例代码为：`ida_idp.get_reg_name(insn.ops[0].reg, ida_ua.get_dtype_size(insn.ops[0].dtype))`

- 汇编转字节码  
https://python.docs.hex-rays.com/idautils/index.html#idautils.Assemble  
idautils.Assemble(ea, "nop")，ea用以指定汇编指令的所在地址，第二个参数指定需要转字节码的汇编指令，该例返回(True, b'\x90')。  
其次，文档提及Assemble接口的第二个参数可以用数组指定多行汇编，例如idautils.Assemble(ea, ["nop","mov eax, eax"])将返回(True, [b'\x90', b'\x89\xc0'])  

- 指令Patch  
ida_bytes.patch_bytes(ea,b"\x90\x90\x90\x90\x90")  
ida_bytes以及idc模块下还有很多patch相关的接口，例如patch_byte，patch_word等等。  
[Search patch in idapython doc](https://python.docs.hex-rays.com/search.html?q=patch&check_keywords=yes&area=default)  

### SDK Example
https://docs.hex-rays.com/9.0sp1/developer-guide/idapython/idapython-examples  

- Dump函数控制流
https://github.com/HexRaysSA/IDAPython/blob/9.0sp1/examples/disassembler/dump_flowchart.py  
遍历函数的所有基本块，以及该基本块的前驱以及后继基本块  

- 列出已修补的字节  
https://github.com/HexRaysSA/IDAPython/blob/9.0sp1/examples/disassembler/list_patched_bytes.py  
列出所有已patch的字节以及对应地址  
