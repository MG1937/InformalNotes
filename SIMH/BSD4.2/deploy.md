# 在Windows上部署BSD4.2
为了编译GCC 0.9, 我们需要一个合适的操作系统, 自由软件之父理查德斯托曼传记<若为自由故>曾提到GNU工程在FreeBSD操作系统以及PDP-10, VAX计算机上开发, 因此, 我们借助SIMH项目模拟VAX780计算机并运行BSD2.4系统.   

关于如何使用SIMH以及安装BSD4.2, 可以参考以下文档, 其中一些磁带文件(.tap文件)的资源可能已经失效, 但证实在WebArchive内拥有备份. VAX780模拟器的产物文件vax780.exe可以在SIMH项目release中发布的zip文件内获取.
[Installing 4.2 BSD on SIMH](https://gunkies.org/wiki/Installing_4.2_BSD_on_SIMH)

需要注意的是SIMH 3.9版本在执行install.ini的过程中会出现如下错误并停止继续执行.
```
Process PTE in P0 or P1 space, PC: 80021C5F (XORW3 @-7035(R8),@D0FEC866,@-70B0(R1))
```
Github上SIMH项目的[issue](https://github.com/simh/simh/issues/480)表示有用户尝试使用4.0.0的SIMH安装BSD4.3时遇到了类似的问题, 最终用户降低SIMH版本, 使用3.8成功启动了BSD4.3, 此处有关BSD4.2的问题同样可以使用此方法解决, SIMH3.7可以成功启动BSD4.2   

# 向BSD4.2传输文件
最开始我尝试使用BSD携带的ftp命令传输文件, 但很快发现BSD与宿主机网络并不相连, 随后我发现了此文用以在BSD4.2上安装网络驱动[4.2BSD on SIMH vax with networking](https://plover.net/~agarvin/4.2bsd.html), 但重点不是驱动, 而是文章向BSD内传输驱动文件的方法.
```shell
sim> at ts0 de_drivers.tape
sim> go

myname cd /tmp
myname# mt rew
myname# tar xvpb 20
```
[SIMH VAX780 文档](https://simh.trailing-edge.com/pdf/vax780_doc.pdf)
[UNIX BSD 文档](http://chuzzlewit.co.uk/utp_book-1.1.pdf)
如上是从网络驱动安装文章中提取出的命令, 以及另外两篇有关VAX780计算机与BSD操作系统的文档, 借助这两篇文档足以理解文章中的命令. 首先利用att命令将de_drivers.tape磁带文件挂在到ts0存储单元, VAX780文档指出ts类型的存储单元用以模拟磁带读写.   

接着执行go之类继续运行BSD4.2, 在系统内执行mt rew以执行磁带倒带, 让读写指针倒回到磁带最开始, 接着借助tar命令解压磁带文件内的tar文件, 此处tar命令没有指定明确的被读取目标, 但系统会默认从磁带设备内读取并解压文件, 实际上这一步可以通过dd命令从/dev/mt0内读取到完整tar文件, 通过BSD的文档可以知道/dev/mt0通常指向磁带驱动设备.   

其次是为什么tar命令可以直接解压磁带内容, 这是因为de_drivers.tape内存储了tar文件, 或者说将tar文件转化为了磁带文件, 而我们也可以利用SIMH BSD安装文档内提供的mkdisttap.pl脚本, 修改脚本中的操作目标以转换指定tar文件为tap磁带文件, 这么一来我们就可以挂载自己的文件了.

下文是一个示例, 我们通过此方法挂载gcc 0.9的项目源码.
```shell
sim> att ts0 gcc09.tap
sim> go
ls
myname# tar xvp
x gcc-0.9/.gdbinit, 573 bytes, 2 tape blocks
x gcc-0.9/dbxout.c, 23737 bytes, 47 tape blocks
x gcc-0.9/cse.c, 67737 bytes, 133 tape blocks
x gcc-0.9/internals.aux, 2844 bytes, 6 tape blocks
x gcc-0.9/explow.c, 14604 bytes, 29 tape blocks
x gcc-0.9/tree.def, 22858 bytes, 45 tape blocks
x gcc-0.9/m68000-output.c, 6577 bytes, 13 tape blocks
...
```

# GCC0.9 的其它编译条件
通过gcc0.9的Makefile文件我们可知, 要编译gcc还需要另一个二进制文件bison, 幸运的是, 我们通过此文档[Building and using GCC 0.9 aka the first public version](https://virtuallyfun.com/2016/12/01/building-using-gcc-0-90-aka-first-public-version/)得知文档作者已经编译了bison并将其制作为磁带文件, 该磁带已被上传至sourceforge以供下载, 可以从文档底部内找到相关链接.
```shell
myname# ln -s /usr/local/bin/bison bison
```
在挂载bison的磁带文件后, 通过tar命令会将bison解压至/usr/local/bin/bison, 此时需要进入/bin目录并通过软连接使得bison成为全局可执行的命令, 随后编译gcc便不会出错且可以正常执行.
