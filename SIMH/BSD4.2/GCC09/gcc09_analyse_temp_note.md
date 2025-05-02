# GCC0.9 分析草稿
顾名思义, 此档非正式的研究记录, 而是分析草稿, 用以暂时记录零散的笔记, 分析进度.  
规定本档的所有分析草稿以如下Markdown格式记录:  
```md
## 分析内容主题
${timestamp}  
详细的分析草稿
```
  
## GCC Resource
20250421  
Essential Abstractions in GCC '13  
GCC基本抽象, 此讲义由印度孟买理工大学提供, 在Youtube内拥有对应的课程录像.  
https://web.archive.org/web/20211026060024/http://www.cse.iitb.ac.in/grc/gcc-workshop-13/  
https://web.archive.org/web/20151001054836/http://www.cse.iitb.ac.in/grc  
https://web.archive.org/web/20201025050346/http://www.cse.iitb.ac.in/grc/index.php?page=docs  

## Compiler Spec
20250421
```cpp
struct compiler
{
  char *suffix;			/* Use this compiler for input files
				   whose names end in this suffix.  */
  char *spec;			/* To use this compiler, pass this spec
				   to do_spec.  */
};

...

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
compiler结构内的spec成员用来指定编译标准, suffix成员指定该标准适用的后缀文件, 在gcc.c内, compilers指定了.c与.s的编译标准.  
这意味着当.c或.s后缀文件被gcc编译时, gcc会根据编译标准格式化特定的编译命令来处理文件.  
编译标准在GNU官方文档内有相关记录: https://gcc.gnu.org/onlinedocs/gcc/Spec-Files.html  

## CC1 目标文件
20250422  
由于VAX780架构早已不通用, 因此预期情况下我们只关注GCC的汇编生成阶段, 而不关注指令生成或者链接阶段.  
```
myname# gcc -v test.c -o test
 cpp -Dvax test.c /tmp/cc020322.cpp
 cc1 /tmp/cc020322.cpp -quiet -dumpbase test.c -noreg -o /tmp/cc020322.s
 as -o test.o /tmp/cc020322.s
 ld -o test /lib/crt0.o test.o -lc
```
通过向gcc添加-v参数来显示编译细节, 可以看到编译过程中的cc1阶段生成了.s文件, 显然这是汇编文件.
```
myname# cc1 test.c -o test.s

test.c:1: undefined or invalid # directive
 main
time in parse: 0.000000
time in expand: 0.000000
time in jump: 0.000000
time in cse: 0.000000
time in loop: 0.000000
time in flow: 0.000000
time in combine: 0.000000
time in local-alloc: 0.000000
time in global-alloc: 0.010000
time in final: 0.000000
time in varconst: 0.000000
time in symout: 0.000000
time in dump: 0.000000
myname# ls
test    test.c  test.s
myname# cat test.s
.globl _main
.text
        .align 0
LC0:
        .asciz "Hello gcc0.9\012"
.text
        .align 1
_main:
        .word 0x0
        pushab LC0
        calls $1,_printf
        clrl r0
        ret
```
手动构造cc1命令并编译测试test.c, 可以看到命令生成了对应的汇编文件, 并且文件内有完整的VAX780架构指令.  
通过查看Makefile可知, cc1是整个gcc0.9编译时的单独编译目标, 主文件gcc只链接了gcc.o, 而cc1则链接了35个.o文件.  
这些.o文件如下所示, 通过其文件名可以大致猜测这些目标文件可能作用于编译前端.
```make
OBJS = toplev.o parse.tab.o tree.o print-tree.o \
 decl.o typecheck.o stor-layout.o fold-const.o \
 varasm.o rtl.o expr.o stmt.o expmed.o explow.o optabs.o \
 symout.o dbxout.o emit-rtl.o insn-emit.o \
 jump.o cse.o loop.o flow.o stupid.o combine.o \
 regclass.o local-alloc.o global-alloc.o reload.o reload1.o \
 final.o recog.o insn-recog.o insn-extract.o insn-output.o

cc1: $(OBJS) $(OBSTACK1)
	ld -o cc1 /lib/crt0.o $(OBJS) -lg $(LIBS) -lc
```
继续分析发现这35个.o文件中, 有部分是通过Makefile里的其它目标文件生成的, 比如insn-emit.c在原始的gcc0.9源码中是不存在的.  
但insn-emit.c在Makefile中作为目标文件存在, 且其生成前提要求md与genemit两个文件均存在或至少一者发生更新.  
```make
insn-emit.c : md genemit
	genemit md > insn-emit.c
```
同时可以发现, genemit并非BSD4.2的自带命令, 其在Makefile中又以另一个目标文件的形式存在, 许多源代码文件在Makefile中的生成过程是层层套叠的, 因此为了方便观察, 直接通过make命令再次编译一次gcc, 同时观察哪些命令被执行过.  
```
myname# make
cc -c -g -I. gcc.c
cc -g -I. -c obstack.c
ld -o gcc /lib/crt0.o gcc.o -lg obstack.o  -lc
cc -g -I. -c toplev.c
bison -v parse.y
parse.y contains 12 shift/reduce conflicts.
cc -g -I. -c parse.tab.c
cc -g -I. -c tree.c
cc -g -I. -c print-tree.c
cc -g -I. -c decl.c
cc -g -I. -c typecheck.c
cc -g -I. -c stor-layout.c
cc -g -I. -c fold-const.c
cc -g -I. -c varasm.c
cc -g -I. -c rtl.c
cc -c -g -I. genflags.c
cc -o genflags -g genflags.o rtl.o obstack.o
genflags md > insn-flags.h
cc -c -g -I. gencodes.c
cc -o gencodes -g gencodes.o rtl.o obstack.o
gencodes md > insn-codes.h
cc -g -I. -c expr.c
cc -g -I. -c stmt.c
cc -g -I. -c expmed.c
cc -g -I. -c explow.c
cc -c -g -I. genconfig.c
cc -o genconfig -g genconfig.o rtl.o obstack.o
genconfig md > insn-config.h
cc -g -I. -c optabs.c
cc -g -I. -c symout.c
cc -g -I. -c dbxout.c
cc -g -I. -c emit-rtl.c
cc -c -g -I. genemit.c
cc -o genemit -g genemit.o rtl.o obstack.o
genemit md > insn-emit.c
cc -c -g -I. insn-emit.c
cc -g -I. -c jump.c
cc -g -I. -c cse.c
cc -g -I. -c loop.c
cc -g -I. -c flow.c
cc -g -I. -c stupid.c
cc -g -I. -c combine.c
cc -g -I. -c regclass.c
cc -g -I. -c local-alloc.c
cc -g -I. -c global-alloc.c
cc -g -I. -c reload.c
cc -g -I. -c reload1.c
cc -g -I. -c final.c
cc -g -I. -c recog.c
cc -c -g -I. genrecog.c
cc -o genrecog -g genrecog.o rtl.o obstack.o
genrecog md > insn-recog.c
cc -c -g -I. insn-recog.c
cc -c -g -I. genextract.c
cc -o genextract -g genextract.o rtl.o obstack.o
genextract md > insn-extract.c
cc -c -g -I. insn-extract.c
cc -c -g -I. genoutput.c
cc -o genoutput -g genoutput.o rtl.o obstack.o
genoutput md > insn-output.c
cc -c -g -I. insn-output.c
ld -o cc1 /lib/crt0.o toplev.o parse.tab.o tree.o print-tree.o  decl.o typecheck.o stor-layout.o fold-const.o  varasm.o rtl.o expr.o stmt.o expmed.o explow.o optabs.o  symout.o dbxout.o emit-rtl.o insn-emit.o  jump.o cse.o loop.o flow.o stupid.o combine.o  regclass.o local-alloc.o global-alloc.o reload.o reload1.o  final.o recog.o insn-recog.o insn-extract.o insn-output.o -lg obstack.o  -lc
cc -g -I. -c cccp.c
echo expect 40 shift/reduce conflicts
expect 40 shift/reduce conflicts
yacc cexp.y

conflicts: 40 shift/reduce
cc -g -I. -c y.tab.c
cc -o cccp -g cccp.o y.tab.o
rm -f cpp
ln cccp cpp
```
同时需要注意, md这个文件是由gcc0.9源码内对应架构的.md后缀文件链接而来, 例如VAX架构下需要执行`ln -s vax.md md`.  
相关的编译前提细节均在此资源内有描述: https://gist.github.com/olegslavkin/2966ae0580873f57e46fb38255e82db3  

## GCC RTL
20250502  
为什么要研究RTL? 在迁移GCC09架构从VAX至X86-64的工作中(此工作详见另一份草案), debug指令生成的过程需要频繁通过gdb访问`rtl->fld(rtunion)`结构以及emit-rtl.c源码文件, 并且整个过程与RTX/RTL概念有多次交集, 因此, 为了方便后续的GCC编译过程研究, 有必要先对RTL进行系统性认知与学习. 如下印度孟买理工学院提供的CS715课件, Uday Khedker在Introduction to RTL的第八页明确提到机器汇编生成前的一个环节是RTL表示优化, 这进一步证明了两者的密切关联.  
https://www.cse.iitb.ac.in/~uday/courses/cs715-09/gcc-rtl.pdf  

其它有关RTL的资料如下:  
* GCC官方文档 RTL Representation: https://gcc.gnu.org/onlinedocs/gcc-3.3/gccint/RTL.html  
* 简述RTL表示语法的台湾Blog, 简明扼要地描述了RTL的关键元素: https://kitoslab.blogspot.com/2012/06/gcc-rtl-insn.html  
* HackMD文档 GCC Internals - RTL Insn: https://hackmd.io/@hauhsu/ryCc8GBW1l?utm_source=preview-mode&utm_medium=rec  
* GCC基本抽象'13 印度孟买理工课件: https://web.archive.org/web/20220308075642/http://www.cse.iitb.ac.in/grc/gcc-workshop-13/downloads/slides/Day3/gccw13-rtl-intro.pdf  

下面以GCC0.9的`cc1 -dc`命令输出的.combine文件为例, 此文件包含.c源码经过编译器前端解析后产生的RTL表达式, 下面是源码`int a = 6;`对应的RTL形式.
```
(insn 3 2 4 (set (reg:SI 16)
       (const_int 6)) -1 (nil)
   (nil))
```
RTL的语法形式参考了Lisp语言, 在分析上面的RTL前, 需要对RTL的一些基本概念进行了解. 首先是RTX Code, 其可以被理解为RTL的基本单元, GCC官方文档没有非常详细地介绍过RTX, 其概念在文档的大多数内容中是隐含的, 但CS715课件的第42页记录了RTX的详细实现, RTX通常由项目代码内rtl.def文件中的DEF_RTL_EXPR宏进行定义, 其由rtl.c/.h文件进行引用以初始化RTX的各项元素, 下面是GCC0.9对于RTX Code "insn"的定义:  
```cpp
/* An instruction that cannot jump.  */
DEF_RTL_EXPR(INSN, "insn", "iuueiee")
```
该宏定义中最重要的一项应当是RTX Format(格式), 也就是字符串"iuueiee", 格式定义了RTX Code其操作数的基本结构, 而关于格式的含义, 在GCC官方文档的"RTL Classes and Formats"章节均有释义. 格式中的每个字符均代表一种类型, 此例中, i代表整数类型, u代表指向其它RTX的指针, e代表另一个RTL表达式. 此外, 格式的每个类型字符的位置与最终RTX以RTL形式呈现时的结构是相互对应的, 这一点在GCC官方文档内也未有提及, 同样是隐含信息. 因此, 此例中的RTL应按照如下形式进行解析:
```
insn format "iuueiee"
(insn ... ) => INSN RTX Code
3 => i(Int)
2 =>  u(RTX Pointer)
4 => u(RTX Pointer)
(set (reg:SI 16) (const_int 6)) => e(RTX Expression)
-1 => i(Int)
(nil) => e(RTX Expression)
(nil) => e(RTX Expression)
```
现在, 我们知道了如何按照格式字符串对RTL进行解析, 但我们仍不知道每个格式类型的对应含义, GCC文档的"Insns"章节阐述了insn RTX Code的解析规范, 首先, 第一个i通常代表当前RTX的位置下标, 例如此例insn的下标位3, 第二个与第三个u代表指向其它下标的RTX指针, 文档描述为链接此insn的前驱以及后继insn, 这意味着insn RTX Code类似链表结构, 第四个e代表指令的具体操作, set insn RTX通常代表其操作数间的赋值操作, 此处暂时不关注, 后三项为代码的注释信息, 我们同样不关注.  
```cpp
#define INSN_UID(INSN)  ((INSN)->fld[0].rtint)
#define PREV_INSN(INSN) ((INSN)->fld[1].rtx)
#define NEXT_INSN(INSN) ((INSN)->fld[2].rtx)
...
```
rtl.h源码文件中的宏定义是对RTX格式解析规范的具体实现, 例如用于获取insn位置下标的宏定义`INSN_UID`是通过访问rtx结构的fld数组实现的, 获取前驱insn RTX Code的宏`PREV_INSN`的实现也同样类似, 不同的是不同的宏使用了不同的下标访问fld数组, 这些下标与RTX格式中每个字符的位置直接关联, 例如代表前驱insn的u属于insn RTX格式中的第二个字符, 那么对应宏访问fld的下标则是1, 显然这符合直觉, 这也说明了为什么debug指令生成过程中需要gdb频繁访问rtx->fld数组中的联合体.  

```
/* Assignment.
   Operand 1 is the location (REG, MEM, PC, CC0 or whatever) assigned to.
   Operand 2 is the value stored there.
   ALL assignment must use SET.
   Instructions that do multiple assignments must use multiple SET,
   under PARALLEL.  */
DEF_RTL_EXPR(SET, "set", "ee")

(set (reg:SI 16) (const_int 6))
```
那么现在再去观察除了insn以外的其它RTX就很好理解了, 例如此例中set RTX的RTL形式, rtl.def给出定义解释是, 此RTX用于将操作数2的值赋值给操作数1, 操作数2很好理解, 是常量6, 其RTX Code为const_int, 操作数1是reg RTX Code, GCC官方文档的"Machine Modes"章节阐述了reg的:SI后缀代表SImode, 用于表示4字节的int类型寄存器, 此寄存器编号为16.  

最终, 这整个insn的RTL可被解读为: 一个指令insn RTX Code, 其当前下标为3, 与其相连的前驱指令下标为2, 后继指令下标为4, 该指令的具体操作为将常量值6赋值给编号为16的4字节Int类型寄存器.  

有关RTL的更详细解读, 可见GCC基本抽象'13课件的第18页.  

