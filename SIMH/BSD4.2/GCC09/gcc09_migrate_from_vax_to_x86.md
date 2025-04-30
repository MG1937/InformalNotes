# Migrating GCC-0.9 From VAX To X86 Structure
> Time: 20250427  
> 本档原属于GCC0.9分析草案的一部分, 由于篇幅原因, 本档与草案文档独立.  
> 由理查德斯托曼释于1987年的C语言编译器GCC0.9原只适用于VAX780架构计算机内的BSD4.2系统, 但此系统仅能运行功能欠缺的GDB2.0, 且系统的csh命令行解释器同样欠缺大量功能, 这使得整体对GCC的研究极为不便.  
> 因此, 出于未来的调试效率考虑, 我们抽出一些时间将GCC0.9的部分功能迁移至现代的X86设备上.

注意, 本次迁移目标的机器信息如下:   
`Linux ubuntu 6.5.0-44-generic #44~22.04.1-Ubuntu SMP PREEMPT_DYNAMIC Tue Jun 18 14:36:16 UTC 2 x86_64 x86_64 x86_64 GNU/Linux`

# CC1 as Target
```cpp
struct compiler compilers[] =
{
	/* Note that we use the "cc1" from $PATH. */
  {".c",
   "cpp %{C} %p %{pedantic} %{D*} %{U*} %{I*} %i %{!E:%g.cpp}\n\
%{!E:cc1 %g.cpp -quiet -dumpbase %i %{Y*} %{d*} %{m*} %{w} %{pedantic}\
		     %{O:-opt}%{!O:-noreg}\
		     %{g:-G}\
		     -o %{S:%b}%{!S:%g}.s\n\
     %{!S:as %{R} -o %{!c:%d}%w%b.o %g.s\n }}"},
  {".s",
   "%{!S:as %{R} %i -o %{!c:%d}%w%b.o\n }"},
  /* Mark end of table */
  {0, 0}
};
```
以上是GCC0.9对.c文件的编译规范, 由前置分析可知gcc.c本身充当命令解析以及编译流程分配的角色, 那么从编译规范中可以知道gcc.c指定编译产物cc1将.c文件从高级语言解析至机器汇编, 随后指定编译产物as作为汇编器, 将机器汇编编译为机器指令.  
同样的, 在草案中我们提到仅关注汇编生成阶段, 而不关注指令编译或者链接, 因此, 选择cc1作为当前的主要迁移目标.  

# Makefile
```makefile
cc1: $(OBJS) $(OBSTACK1)
	ld -o cc1 /lib/crt0.o $(OBJS) -lg $(LIBS) -lc
```
首先需要调整Makefile内的部分目标文件编译配置, crt0.o(C Runtime)是程序编译时必要的目标文件, 它提供了一个\_start函数入口, 并在运行时进行必要的初始化工作, 使得系统可以顺利进入程序的main入口, 并配置对应的运行环境, 缺少crt0.o会导致最终编译产物丢失解释器, 或使执行流无法被准确引导至函数入口.  
```
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# find / -name crt*.o
/usr/lib/x86_64-linux-gnu/crtn.o
/usr/lib/x86_64-linux-gnu/crti.o
/usr/lib/x86_64-linux-gnu/crt1.o
```
BSD4.2系统的crt0.o文件默认存储于/lib目录下, 而迁移目标机器并不存在此文件, 通过find命令搜索crt\*.o可以发现/usr/lib目录下的crt1.o文件, 因此我们替换Makefile中的所有crt0.o为该目录下的crt1.o, 后者为现代编译器演化的产物, 其作用与crt0.o基本一致.  
```makefile
parse.tab.c : parse.y
	bison -v parse.y
...
insn-flags.h : md genflags
	genflags md > insn-flags.h
```
其次是由其它二进制文件自动生成的源码文件, 它们不包含在原始gcc0.9项目内, 例如parse.tab.c文件是由bison语法分析生成器解析.y文件主动生成, 虽然迁移目标机器的系统上也拥有bison命令, 但经过测试, 此命令在解析早期的parse.y时会产生语法错误, 导致源码文件输出失败. 这里我们采用的解决方案是, 在Makefile内注释可能产生编译失败的目标文件, 随后在原BSD4.2系统上编译gcc0.9项目, 将编译中程自动生成的所有额外源码文件打包为tar, 并传入目标机器内, 经过证实, 在BSD4.2上产生的源码文件可在新机器上复用.  
另外, 除了bison命令, 其它用于自动生成源码文件的二进制产物例如genflags, 可以通过修改对应的源码文件(例如genflags.c)中的运行错误成功生成目标源码文件.  

# We Choose X86 Structure
```cpp
Program received signal SIGSEGV, Segmentation fault.
_obstack_begin (h=0x55555555b820 <obstack>, chunkfun=0x555555556674 <xmalloc>) at obstack.c:110
110        = (char *) chunk + h->chunk_size;
(gdb) p chunk
$1 = (struct obstack_chunk *) 0x5555c2a0
(gdb) x chunk
0x5555c2a0:     Cannot access memory at address 0x5555c2a0
(gdb) list xmalloc
118       printf ("extern rtx gen_%s ();\n", XSTR (insn, 0));
119     }
120     ^L
121     int
122     xmalloc (size)
123     {
124       register int val = malloc (size);
125
126       if (val == 0)
127         abort ();
```
运行在VAX架构上的BSD4.2系统通常使用DWORD长度的虚拟地址, 因此, 简单审视GCC0.9项目上下文, 可以发现大部分用以操作指针的代码均以int字段存储指针类型引用, 在简单make项目后, 运行其中一个二进制产物, 程序随即出现段错误.  
使用gdb进行定位, 错误原因显然, obstack会使用chunk中携带的chunkfun函数指针进行内存分配相关的操作, 本次错误发生在chunkfun指向xmalloc函数时, \_obstack_begin函数首先通过xmalloc为chunk分配空间, 并从其返回中获取该空间的虚拟地址, 然而, 由于早期obstack中的xmalloc函数以int类型返回分配的地址, 而我们在目标机器上获得的编译产物为64位架构, 这使得xmalloc错误地将原本QWORD长度的虚拟地址截断为DWORD长度并返回, 使得程序无法从被截断的错误地址内读取内存, 从而导致段错误.  

事实上, 远不止obstack这一处逻辑使用了int指代指针类型, 将GCC0.9项目内的所有指针类型进行修复是个极庞大的工作量, 这显然不合理, 因此, 将编译产物指定为32位架构以适应指针以及虚拟地址问题是相对合理的.  

https://stackoverflow.com/questions/54082459/fatal-error-bits-libc-header-start-h-no-such-file-or-directory-while-compili  
为了使32位架构的编译工作能够顺利进行, 我们需要通过`apt-get install gcc-multilib`命令额外部署32位标准库, 并在Makefile内指定"CC=cc -m32", 通过-m32选项以指定32位的编译架构. 最终, 地址相关的问题被大部分缓解.

# Syntax Issue
```cpp
cc -m32  -w -g -I.    -c -o jump.o jump.c
jump.c:329:1: error: static declaration of ‘simplejump_p’ follows non-static declaration
  329 | simplejump_p (insn)
      | ^~~~~~~~~~~~
jump.c:177:27: note: previous implicit declaration of ‘simplejump_p’ with type ‘int()’
  177 |                        && simplejump_p (reallabelprev))
      |                           ^~~~~~~~~~~~
jump.c:346:1: error: static declaration of ‘condjump_p’ follows non-static declaration
  346 | condjump_p (insn)
      | ^~~~~~~~~~
jump.c:166:44: note: previous implicit declaration of ‘condjump_p’ with type ‘int()’
  166 |               if (reallabelprev == insn && condjump_p (insn))
      |                                            ^~~~~~~~~~
make: *** [<builtin>: jump.o] Error 1
```
GCC0.9由现代编译器进行编译时, 会产生某些语法错误, 其中最经常出现的当属如上报错所示. 早期的K&R C语法允许extern与static关键字同时修饰一个函数或字段, 即便二者的的作用完全相反, extern可以指定目标允许被外部文件链接, 而static修饰则强制指定目标为内部链接, 且无法被外部访问, 因此使用作用冲突的关键字修饰同一个目标是现代编译器绝不允许的. 不过此处的修复方案也很简单, 仅需删除抛错函数的static关键字, 允许其被外部链接即可, 不过出现该错误的源代码文件略多, 需要仔细排查.  
有关二者关键字的具体作用,详见CppReference表述: https://en.cppreference.com/w/cpp/language/storage_duration  

# Success Migrating
前面的章节提到要将编译架构改动为32位, 此章将对Makefile文件进行最后的更改, 最终完成从VAX到x86机器的迁移. 首先, 在通过apt install命令完成32位标准库的部署后, 需要对Makefile中指定的crt1.o目标文件进行明确, 通过find命令可以找到/usr/lib32/crt1.o, 需要在目标文件cc1的编译配置中添加此编译文件.  
```
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# ld -o cc1 /usr/lib32/crti.o /usr/lib32/crtn.o /usr/lib32/crt1.o toplev.o parse.tab.o tree.o print-tree.o decl.o typecheck.o stor-layout.o fold-const.o varasm.o rtl.o expr.o stmt.o expmed.o explow.o optabs.o symout.o dbxout.o emit-rtl.o insn-emit.o jump.o cse.o loop.o flow.o stupid.o combine.o regclass.o local-alloc.o global-alloc.o reload.o reload1.o final.o recog.o insn-recog.o insn-extract.o insn-output.o obstack.o  -lc -lg
ld: symout.o: in function `symout_finish':
/root/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9/symout.c:982: warning: the `getwd' function is dangerous and should not be used.
ld: i386 architecture of input file `/usr/lib32/crti.o' is incompatible with i386:x86-64 output
ld: i386 architecture of input file `/usr/lib32/crtn.o' is incompatible with i386:x86-64 output
ld: i386 architecture of input file `/usr/lib32/crt1.o' is incompatible with i386:x86-64 output
...
```
然而在链接过程中, 出现了如上抛错, 提示参与编译的文件为i386架构, 但目标产物为x86-64架构, 二者不匹配, 因此还需要添加链接选项`-m elf_i386`以指定最终产物的架构.  
```
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# ldd cc1
        linux-gate.so.1 (0xf35d7000)
        libc.so.6 => /lib32/libc.so.6 (0xf3200000)
        /usr/lib/libc.so.1 => /lib/ld-linux.so.2 (0xf35d9000)
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# ./cc1
bash: ./cc1: No such file or directory
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# ls /usr/lib/libc.so.1
ls: cannot access '/usr/lib/libc.so.1': No such file or directory
```
但此时还未结束, 即便指定了编译架构, 使用ldd命令解析最终产物的动态连接器(解释器), 可以看见输出中的`/usr/lib/libc.so.1 => /lib/ld-linux.so.2`信息, 即便输出信息提示了/usr/lib/libc.so.1链接到最终的/lib/ld-linux.so.2, 但通过ls命令查看此libc.so.1发现文件并不存在, 因此还需要添加链接选项--dynamic-linker手动指定解释器, 否则最终产物无法运行.  
```
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# make
ld -o cc1 /usr/lib32/crt1.o toplev.o parse.tab.o tree.o print-tree.o decl.o typecheck.o stor-layout.o fold-const.o varasm.o rtl.o expr.o stmt.o expmed.o explow.o optabs.o symout.o dbxout.o emit-rtl.o insn-emit.o jump.o cse.o loop.o flow.o stupid.o combine.o regclass.o local-alloc.o global-alloc.o reload.o reload1.o final.o recog.o insn-recog.o insn-extract.o insn-output.o obstack.o  -lc -lg -m elf_i386 --dynamic-linker /lib/ld-linux.so.2
ld: symout.o: in function `symout_finish':
/root/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9/symout.c:982: warning: the `getwd' function is dangerous and should not be used.
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# ./cc1
cc1: No such file or directory
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# ./cc1 test.c -o test.s
 main
time in parse: 0.000094
time in expand: 0.000034
time in jump: 0.000000
time in cse: 0.000000
time in loop: 0.000000
time in flow: 0.000015
time in combine: 0.000000
time in local-alloc: 0.000016
time in global-alloc: 0.000017
time in final: 0.000148
time in varconst: 0.000015
time in symout: 0.000000
time in dump: 0.000000
root@ubuntu:~/gcc/gcc-0.9-build/gcc-0.9-unfix/gcc-0.9# cat test.s
.globl _main
.text
        .align 1
_main:
        .word 0x0
        movl 4(ap),r0
        ashl $1,r0,r1
        addl2 r1,r0
        ret
```
如上输出, 最终cc1被成功编译在x86机器上, 且正常运行.

# Postscript
后记, 实际上cc1的修复过程并不如此档描述地如此轻松, 由于我最初错误地判断需要将最终产物编译为64位架构, 导致中途出现了许多错误, 譬如前面提到的由于架构导致的虚拟地址或指针长度截断问题, 让obstack无法正常运行, 具体的现象例如同一个chunk的指针被不同模块共同使用或者覆盖, 这显然不是预期行为(正常来说应该是一个模块被obstack分配一个chunk内存, 而不是同用一段空间), 从而导致cc1运行过程中的某些数据出现错误, 后来为了解决这些问题, 我手动将obstack的宏定义引向malloc, 以避免这个问题.  

但虚拟地址长度问题影响的远不止obstack, 比如后续用以生成指令的中间表示rtx结构的函数emit-rtl.c#gen_rtx也受到架构问题影响, gen_rtx内部通过栈的形式取数量未知的参数, 但架构变动导致调用约定变动, 使得取参过程变得不可控, 导致未定义行为, 后续我是通过手动拓展gen_rtx参数数量, 并分别为每个指令定义指定参数作为操作数解决的, 还有很多折磨人的修复过程, 比如结构莫名其妙偏移了几个字节, 就不提及了. 虽然过程不如直接改动编译架构为32位那样来得顺利, 但最终我居然真的成功编译出了gcc0.9的64位架构版本, 并成功运行了!  

虽然对比32位编译如此顺利的情况, 当初决定强制编译64位绝对是一个决策错误, 但这个过程与消耗掉的时间并非无用功, 我通过在64位架构内解决这些奇怪且折磨人的疑难杂症, 使得我在正式分析gcc0.9之前提前地学习了gcc的结构, 编译过程以及指令生成过程, 也算是一种有趣的收获吧.

