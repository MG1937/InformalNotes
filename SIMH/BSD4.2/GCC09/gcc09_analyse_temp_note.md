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

