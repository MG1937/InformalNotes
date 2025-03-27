# gnu_debugdata 段调试信息解析
理论上nm, readelf, objdump诸如此类的命令可以解析出的符号, 应当在字符串表内也可以找到, 然而, 在某些情况下, 可能会出现调试器可获取符号, 但二进制的字符串表内无法得到符号的情况, 这对寻找感兴趣的欲调试对象带来不便.   

以二进制surfaceflinger为例, 假设感兴趣的目标符号为”ackWindowInfosReceived”, 但通过命令`readelf -Ws surfaceflinger | grep ackWindowInfosReceived`却无法获取该符号.   

但利用lldb在对应位置下断点并bt列出调用堆栈, 却发现lldb可以获取详细的堆栈信息, 包括ackWindowInfosReceived符号.   
```
* thread #16, name = 'surfaceflinger', stop reason = breakpoint 4.1
  * frame #0: 0x0000005f2d21a0e8 surfaceflinger`std::__1::__function::__func<android::WindowInfosListenerInvoker::ackWindowInfosReceived(long, long)::$_0, std::__1::allocator<android::WindowInfosListenerInvoker::ackWindowInfosReceived(long, long)::$_0>, void ()>::operator()() (.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887)
    frame #1: 0x0000005f2d1d444c surfaceflinger`android::BackgroundExecutor::BackgroundExecutor()::$_0::operator()() const (.__uniq.40551338734768535425960610675403556019) + 432
    frame #2: 0x0000005f2d1d427c surfaceflinger`void* std::__1::__thread_proxy<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, android::BackgroundExecutor::BackgroundExecutor()::$_0> >(void*) (.__uniq.40551338734768535425960610675403556019.llvm.2068811780102433292) + 48
    frame #3: 0x0000006fe9f446ac libc.so`__pthread_start(void*) + 212
    frame #4: 0x0000006fe9ee1220 libc.so`__start_thread + 68
```

事实上, 这是由于AOSP的编译工具链将一些代码的调试信息以xz格式压缩至gnu_debugdata段内了. 这种运行时解压的调试信息也称作minidebuginfo.   
```
readelf -S surfaceflinger | grep gnu_debugdata
  [28] .gnu_debugdata    PROGBITS         0000000000000000  007834cc
       000000000005c778  0000000000000000           0     0     1
```
通过dd命令将二进制中的gnu_debuginfo段提取出来, 并利用xz命令进行解压, 可以发现最终它是一个ELF文件, 对解压后的文件进行符号查询, 可以查询到原本被压缩的调试信息.   
```
dd if=surfaceflinger of=surfaceflinger.gnudata.xz bs=1 skip=0x007834cc count=0x000000000005c778
xz -d surfaceflinger.gnudata.xz
readelf -Ws surfaceflinger.gnudebug | grep ackWindowInfosReceived
  1499: 00000000001b77b4     4 FUNC    GLOBAL DEFAULT    1 _ZNSt3__110__function6__funcIZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEllE3$_0NS_9allocatorIS4_EEFvvEE7destroyEv.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887
  5305: 00000000002dc6d8     4 FUNC    GLOBAL DEFAULT    1 _ZNSt3__110__function6__funcIZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEllE3$_0NS_9allocatorIS4_EEFvvEE18destroy_deallocateEv.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887
  5306: 00000000002dc6d8     4 FUNC    GLOBAL DEFAULT    1 _ZNSt3__110__function6__funcIZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEllE3$_0NS_9allocatorIS4_EEFvvEED0Ev.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887
  7000: 000000000031fdac   392 FUNC    GLOBAL DEFAULT    1 _ZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEll
  7075: 0000000000322088    64 FUNC    GLOBAL DEFAULT    1 _ZNKSt3__110__function6__funcIZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEllE3$_0NS_9allocatorIS4_EEFvvEE7__cloneEv.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887
  7076: 00000000003220c8    32 FUNC    GLOBAL DEFAULT    1 _ZNKSt3__110__function6__funcIZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEllE3$_0NS_9allocatorIS4_EEFvvEE7__cloneEPNS0_6__baseIS7_EE.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887
  7077: 00000000003220e8  2400 FUNC    GLOBAL DEFAULT    1 _ZNSt3__110__function6__funcIZN7android26WindowInfosListenerInvoker22ackWindowInfosReceivedEllE3$_0NS_9allocatorIS4_EEFvvEEclEv.__uniq.12778241153716518025122897992089108778.b6c03e83aecd928bc80ad22f4346b887
```

可以利用这些信息编写脚本, 在IDA内恢复符号以协助分析.   
https://gist.github.com/rpw/caf82a1b657011cadd7c8b8664f10204

gnu_debugdata解析脚本
```python
import argparse
import subprocess
import re
import os

def main():
    # 1. 获取命令行参数输入
    parser = argparse.ArgumentParser(description="Extract and decompress .gnu_debugdata section from a library.")
    parser.add_argument("lib", help="Path to the library file.")
    args = parser.parse_args()
    lib_path = args.lib

    # 检查文件是否存在
    if not os.path.isfile(lib_path):
        print(f"Error: The file '{lib_path}' does not exist.")
        return

    # 2. 执行 objdump 获取 .gnu_debugdata 段信息
    try:
        result = subprocess.run(
            ["objdump", "-h", lib_path],
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to execute objdump. {e}")
        return

    # 3. 解析输出，获取 offset 和 size
    match = re.search(r'\.gnu_debugdata\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)', result.stdout)
    if not match:
        print("Error: .gnu_debugdata section not found in the provided library.")
        return

    size = int(match.group(1), 16)
    offset = int(match.group(2), 16)

    # 4. 使用 dd 提取 .gnu_debugdata 段
    output_file = f"{lib_path}.gnudata.xz"
    try:
        with open(output_file, 'wb') as out_f, open(lib_path, 'rb') as in_f:
            in_f.seek(offset)
            out_f.write(in_f.read(size))
    except IOError as e:
        print(f"Error: Failed to extract .gnu_debugdata section. {e}")
        return

    print(f"Extracted .gnu_debugdata to {output_file}")

    # 5. 解压缩提取的文件
    try:
        subprocess.run(["xz", "-d", output_file], check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to decompress {output_file}. {e}")
        return

    print(f"Decompressed {output_file} to {output_file[:-3]}")

if __name__ == "__main__":
    main()
```
