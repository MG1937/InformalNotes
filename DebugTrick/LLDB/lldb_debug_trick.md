# LLDB 调试技巧
> 本档致力于持续更新一份随查随用的Manual   
> 让LLDB用起来和WinDBG一样顺手

## How to Setup
1. AndroidStudio SDK Manager -> SDK Tools -> NDK download   
2. SDK directory -> lldb-server   
3. SDK directory -> lldb.cmd   

## Module search
```
(lldb) image list libgui.so
[  0] EA7500B8-6406-B008-D4DA-07426B603D8A 0x00000078e2658000 C:\Users\Admin\.lldb\module_cache\remote-android\.cache\EA7500B8-6406-B008-D4DA-07426B603D8A\libgui.so
``` 

## Symbol VA address
搜索符号相对偏移
```
(lldb) image lookup -r -n ackWindow
1 match found in C:\Users\Admin\.lldb\module_cache\remote-android\.cache\EA7500B8-6406-B008-D4DA-07426B603D8A\libgui.so:
        Address: libgui.so[0x000000000015a5e8] (libgui.so.PT_LOAD[1]..text + 861672)
        Summary: libgui.so`android::gui::BpWindowInfosPublisher::ackWindowInfosReceived(long, long)
```
使用表达式获取绝对虚拟地址
```
(lldb) p (void*)(0x000000000015a5e8+0x00000078e2658000)
(void *) 0x00000078e27b25e8
```

## Quick add breakpoint
b命令可以同时对大量位置进行断点设置, 例如`b ABC`, 此指令将对所有包含'ABC'字符串的符号进行断点设置.
```
(lldb) b BpWindowInfosPublisher::ackWindowInfosReceived
Breakpoint 3: where = libgui.so`android::gui::BpWindowInfosPublisher::ackWindowInfosReceived(long, long), address = 0x00000078e27b25e8
```
命令`breakpoint set -a 0x123456`可对特定地址进行断点设置
```
(lldb) breakpoint set -a 0x00000078e27b25e8
Breakpoint 4: where = libgui.so`android::gui::BpWindowInfosPublisher::ackWindowInfosReceived(long, long), address = 0x00000078e27b25e8
```

## Stop-Hook Auto Continue
在附加system_server后, binder会由于某些不明原因频繁产生SIGSTOP或SIGSEGV信号, 若不希望调试器捕获此信号导致挂起, 同时不影响正常的断点挂起功能, 可借助`target stop-hook`以及`breakpoint command add`命令.   
```
->  0x72c93308 <+712>: ldr    x21, [x21]
    0x72c9330c <+716>: b      0x72c93110     ; <+208>
    0x72c93310 <+720>: adr    x30, 0x72c9331c ; <+732>
    0x72c93314 <+724>: cbnz   x20, 0x72d2e930 ; BakerReadBarrierThunkField_r27_r27_3
  thread #111, name = 'HwBinder:6749_2', stop reason = signal SIGSEGV: invalid address (fault address: 0x0)
    frame #0: 0x0000000072c8f284 boot.oat`android.os.HwParcel.<init> + 20
```

`target stop-hook`命令支持在每次程序挂起时(除了process interrupt的人为挂起)执行特定命令, 使用`target stop-hook add -G true -o "DONE"`命令可以让调试器在每次挂起之际自动放行程序(注意, 这包括breakpoint断点产生的挂起).   

同时, `breakpoint command add`命令支持为特定断点触发时执行特定指令, 可以结合前者在每次挂起时打印关键上下文的堆栈, 且屏蔽由于binder问题导致的停止信号. 如下, lldb在触发断点时自动打印堆栈, 但不挂起进程并继续执行.
```
(lldb)  bt
  thread #11, name = 'binder:10170_2', stop reason = breakpoint 1.1
    frame #0: 0x00000077201cd5e8 libgui.so`android::gui::BpWindowInfosPublisher::ackWindowInfosReceived(long, long)
    frame #1: 0x00000077201af6c8 libgui.so`android::WindowInfosListenerReporter::onWindowInfosChanged(android::gui::WindowInfosUpdate const&) + 180
    frame #2: 0x00000077201ccfec libgui.so`android::gui::BnWindowInfosListener::onTransact(unsigned int, android::Parcel const&, android::Parcel*, unsigned int) + 516
    frame #3: 0x000000773ba67584 libbinder.so`android::BBinder::transact(unsigned int, android::Parcel const&, android::Parcel*, unsigned int) + 252
    frame #4: 0x000000773ba66104 libbinder.so`android::IPCThreadState::executeCommand(int) + 504
    frame #5: 0x000000773ba89c08 libbinder.so`android::IPCThreadState::joinThreadPool(bool) + 584
    frame #6: 0x000000773ba899b0 libbinder.so`android::PoolThread::threadLoop() - 18446743561607538255
```

