# Local ADB
> 20250504  

本草案的原计划是编写一个可以接管Wireless ADB的恶意软件, 用于未来的演示上.  
因此, 我们需要在安卓应用本地部署ADB二进制可执行文件, 而非依赖客户端的Android SDK套件.  
目前, 本草案提供两种方案, 其一是直接通过开源项目LADB提供的编译后的libadb.so执行相关命令, 其二是通过在线的termux包管理服务器自行下载对应deb文件, 自行链接相关的二进制文件.  
前者获取Android本地可执行二进制的方案简便得多, 但考虑到开源项目可能随时被废弃(很多Local ADB的项目都处于废弃状态), 因此长期来看, 有必要记录第二种方案.  

## LADB (推荐)
LADB Github: https://github.com/tytydraco/LADB  
LADB是一款开源的在Android本地部署的adb shell套件, 其源码主要是将libadb.so进行了封装, 经过测试与审计源码上下文, libadb.so实际上并非动态链接库, 而是一个直接可执行的二进制文件, 将其直接从apk内提取并置入/data/local/tmp内, chmod给予执行权限, 是可以直接运行的, 同时, 此项目拥有四种架构的libadb.so, 包括arm64-v8a, armeabi-v7a, x86, x86-64. 
```shell
130|redfin:/data/local/tmp $ ./libadb.so
Android Debug Bridge version 1.0.41
Version 34.0.0-vanzdobz@gmail.com
Installed as /data/local/tmp/libadb.so

global options:
 -a                       listen on all network interfaces, not just localhost
 -d                       use USB device (error if multiple devices connected)
 -e                       use TCP/IP device (error if multiple TCP/IP devices available)
 -s SERIAL                use device with given serial (overrides $ANDROID_SERIAL)
 -t ID                    use device with given transport id
...
```
虽然其二进制文件的最后一次更新在两年以前, 但依然可在Android14系统上运行, 不过需要注意的是, 项目作者并未给出libadb.so的编译过程或者源码, 这意味着一旦任何兼容问题出现, 依赖于libadb.so的工作也将出现问题. 经过简单逆向可以在libadb.so内发现termux相关的调试日志, 因此猜测作者在两年前通过在线的termux包管理服务获取了android-tools的源码, 并进行了编译.

## Termux DEB (备用)
Termux 在线包管理服务: https://packages.termux.dev/  
Termux在Android平台提供了本地可运行的adb可执行文件, 可以通过pkg install命令安装android-tools软件包后, 在其沙箱目录内files/usr/bin内获取adb文件, 或者直接通过在线的包管理服务下载指定的android-tools的deb包, 其后手动解压.  
```shell
1|redfin:/data/local/tmp $ ./adb
CANNOT LINK EXECUTABLE "./adb": library "libprotobuf.so" not found: needed by main executable
```
不过, 无论是通过何种方式获取的android-tools软件包内的adb, 均无法直接运行, 原因是缺少依赖文件. 软件包依赖可在deb内control文件中的依赖列表找到.
```
Package: android-tools
Architecture: aarch64
Installed-Size: 12532
Maintainer: @termux
Version: 35.0.2-2
Homepage: https://developer.android.com/
Depends: abseil-cpp, brotli, libc++, liblz4, libprotobuf, pcre2, zlib, zstd
Description: Android platform tools
```
如上为android-tools的依赖列表, 其中一种依赖获取方法依然是手动从在线包管理服务内获取对应deb包, 解压, 置于adb对应目录下, 随后设置`LD_LIBRARY_PATH`环境变量为当前目录, 当所有依赖全部到位后, 经过测试可以正常运行adb文件. 其二是通过pkg包管理功能正常安装android-tools的所有依赖, 然后从termux的沙箱目录内files/usr/lib内复制对应so文件到指定目录, 接着设置`LD_LIBRARY_PATH`以指定动态库搜索目录. 两种方法原理一致, 均予以记录.  

不过, 前文提到的libadb.so无需其它链接库支持, 可能是由于LADB的作者在编译源码时选择了静态链接, 或者使用了与此章同样的方法提取所有依赖库, 随后使用ld命令手动静态链接所有依赖库, 并输出可执行文件获得. 无论如何, 此节的方案仅作为备用方案使用, 只要termux还在继续维护, 此方案就可以应对未来adb出现更新而导致的其它兼容问题.
