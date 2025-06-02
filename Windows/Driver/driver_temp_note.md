# Manuscript for Windows Driver
20250529  

## Main CoT
目前的学习思路:  
1. 先了解驱动的基本结构, 驱动分类, 驱动功能.  
    * 这能帮助我得知编写驱动的意义, 什么场景下需要使用何种驱动.  
    * 了解驱动间如何相互协作/通讯, 这一点在Driver Stack以及I/O request packets概念中均有提及
2. 学习如何编写一个真正的驱动
    * 直观地感受驱动在工程上的代码结构, 开发一个驱动能帮助我分析其它驱动.
    * 编写某驱动前, 先问问是否真的需要编写, 是否已经有成品? MSDN - Choosing a driver model

驱动基本知识: 
* 驱动分类与驱动栈结构  
* 和驱动通讯需要通过调用IoCallDriver并传递指向DEVICE_OBJECT结构以及IRP的指针
* 内核驱动必须有名为DriverEntry的函数 - minidrivers-and-driver-pairs
* 驱动开发框架有两类, KMDF(内核模式驱动框架)与UMDF(用户模式驱动框架)
    - 是否使用这些驱动框架取决于具体需求, 由于并不是所有驱动技术都有对应的minidriver提供通用功能, 因此实现一些复杂功能, 可能就要使用这些驱动框架.
* 用户模式与内核模式
    - 用户模式下的程式拥有独立的虚拟地址与句柄, 无法修改/访问其它程式/内核的虚拟地址, 但内核模式下的驱动全部共享同一段虚拟地址, 这意味着错误的内存操作很可能使其它驱动产生未定义行为.
* WDK
    - 目前WDK的旧发行版已在4月被微软全面下线, 只能使用最新的WDK编译驱动, 同时, 旧版本的Win10镜像无法部署和运行最新WDK编译的驱动, 需要前往next i tell you下载22H2版本的任意Win10镜像.

名词:  
* PDO: Physical Device Object - Usbstor.sys
* FDO: Functional Device Object - Disk.sys
* Filter DO: 驱动基本知识内提及 Filter Driver 属于辅助类驱动 - Partmgr.sys

命令:
* driverquery: MSDN文档中提及的驱动查询命令
    - 可以直观地查询到minifilter相关驱动 FltMgr
* bcdedit
    - bcdedit /set testsigning on 关闭驱动签名验证
* sc
    - sc create NAME type=kernel binpath=/path/to/driver.sys 创建内核驱动服务
    - sc start NAME 运行驱动服务, 同时触发驱动DriverEntry入口
    - sc stop NAME 停止驱动服务
    - sc delete NAME 卸载驱动服务
* pnputil
    - 可以用于安装驱动, 但不会触发驱动DriverEntry入口, 仅有与驱动相关的设备插入时才会触发入口

资源:  
1. 驱动开发手册
    - https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/  
2. 用户与内核模式详解
    - https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/user-mode-and-kernel-mode
3. 虚拟地址
    - https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/virtual-address-spaces
4. 开源且基于驱动的Window平台EDR
    - https://github.com/ComodoSecurity/openedr
    - https://github.com/dobin/RedEdr
5. WDK下载
    - https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
6. 俄罗斯驱动加载器
    - https://four-f-kmd.github.io/
    - https://code.google.com/archive/p/skillsys/downloads


驱动分类:  
* Device function driver
* Device filter driver
* Software driver
* File system filter driver
* File system driver
来源: MSDN - Choosing a driver model  

Windows支持的自带驱动列表:  
https://learn.microsoft.com/en-us/windows-hardware/drivers/device-and-driver-technologies  

## Target
* 学习驱动基本知识 WIP 20250531 √ 20250601
* 基于minifilter编写驱动 WIP

