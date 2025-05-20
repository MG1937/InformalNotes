# LLDB Script
> 本档用于记录平常编写的一些便于LLDB调试的Python脚本,  
> 以及一些脚本开发资料.  

## LLDB Script Reference
Document: https://lldb.llvm.org/use/python-reference.html  
Script Sample Blog: https://www.ryanipete.com/blog/lldb/python/how_to_create_a_custom_lldb_command_pt_1/  
Github Script Example: https://github.com/llvm/llvm-project/tree/main/lldb/examples/python  
LLDB API Reference: https://lldb.llvm.org/python_reference/  
GeryHat LLDB Script(推荐): https://nusgreyhats.org/posts/writeups/basic-lldb-scripting/  

## LBR for set br quickly
此脚本可以根据模块名称与偏移快速设定断点, 初衷是为了避免重复执行`image list`与计算虚拟地址的繁琐流程.  
用法: `brset <module_name> <offset_hex>`
```python
import lldb

def command_brset(debugger, command, result, internal_dict):
    args = command.split()
    if len(args) != 2:
        print("brset <module_name> <offset_hex>")
        return
    target = debugger.GetSelectedTarget()
    m_name = args[0]
    m_offset = int(args[1], 16)
    for m in target.modules:
        if m_name not in m.GetFileSpec().GetFilename():
            continue
        m_base = m.GetObjectFileHeaderAddress().GetLoadAddress(target)
        debugger.HandleCommand("breakpoint set -a {}".format(m_base + m_offset))
        break

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand("command script add -o -f lldb_brset.command_brset lbr")
```

## Draft Nodes
* `command script import /path/to/script.py`导入并加载指定脚本文件  
* `def __lldb_init_module(debugger, dicts)`可用以在启动时执行一些初始化配置
    - debugger为lldb.SBDebugger类型, 具体细节可查看API文档
    - dicts包含了一些初始化环境参数  
* LLDB设计结构: `lldb -> debugger -> target -> process -> thread -> frame(s)`

