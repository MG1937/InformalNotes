# Inline Code Reverse
本档适用于以下情况，针对完全无源码且不引入任何开源组件的二进制文件可能不适用
1. 无源码的二进制文件引用了开源组件，但组件代码被内联，需要快速分析或定位   
    场景：某二进制文件引用了libbinder.so内对Parcel对象进行处理的接口，但接口实现却被内联，需要快速定位Parcel对象，或快速分析具体的序列化过程
2. 分析完全开源的组件，但由于各种问题无法对组件进行编译，需要在二进制情况下进行分析   
    场景：分析AOSP时需要对照源码对某些文件(apex,so,bin...)进行问题定位，但某些函数被内联，需要快速分析。

## Inline Sample
宏代码、内联函数、模板函数等的代码形式在编译时可能会被展开或生成。宏代码在编译时会被展开为具体的代码，模板函数会在编译时根据模板参数生成具体的实例。内联函数则可能在编译时被替换为函数体本身，以减少函数调用的开销。以LLVM标准库内的std::string实现为例，假设二进制内的某处代码正在处理由std::string管理的代码，那么它的代码形式可能如下所示。
```cpp
      if ( (v24[0] & 1) != 0 )
        v7 = (char *)v24[2];
      else
        v7 = (char *)v24 + 1;
      goto LABEL_14;
```
[libc++'s implementation of std::string](https://joellaity.com/2020/01/31/string.html)   
在阅读这段代码前，你需要对libc++的std::string的内存结构有所了解，如上述链接详细解释了std::string的结构，字符串管理行为，以及短字符串优化(SSO)。现在假定你已经拥有了这方面的前置知识，IDA输出的上面这段伪代码就很好理解了，首先代码检查v24的cap位是否为1，若没有则跳过两个指针长度并直接从堆中提取内容，否则跳过1个byte长度并提取内容，根据这些行为显然我们知道v24是由std::string管理的字符串。   

std::string的实现显然大部分时间被内联在其它函数体内，但联系上下文与代码行为，某些情况下根据经验依然可以辨别出哪些伪代码正在处理字符串。但如果非标准函数或逻辑非常复杂的函数被内联时该怎么办？   

# Inline Code Detect
```cpp
        mDrawingState.traverseInReverseZOrder([&](Layer* layer) {
            if (!layer->needsInputInfo()) return;
            const auto opt =
                    mFrontEndDisplayInfos.get(layer->getLayerStack())
                            .transform([](const frontend::DisplayInfo& info) {
                                return Layer::InputDisplayArgs{&info.transform, info.isSecure};
                            });

            outWindowInfos.push_back(layer->fillInputInfo(opt.value_or(Layer::InputDisplayArgs{})));
        });
```
假定这是正在分析的开源组件的源代码片段，需要在二进制文件内映射对应的代码逻辑，但同时代码被内联且符号被压缩，仅根据肉眼无法在IDA内直接定位对应的代码。此时可以尝试逐步恢复一些符号。LLDB可以直接解析在内存内解压的符号，因此可以借助`image lookup`命令寻找部分符号（当然也可以手动解压符号，并利用readelf逐个读取），并根据这些符号的引用与上下文关系大致确定代码片段的所在地址。如下为示例命令：   
```
(lldb) image lookup -r -v -n fillInputInfo
1 match found in C:\Users\Admin\.lldb\module_cache\remote-android\.cache\35EA916F-138D-F7A6-1BA3-B011780073AF\surfaceflinger:
        Address: surfaceflinger[0x0000000000160670] (gnu_debugdata..text + 112240)
        Summary: surfaceflinger`android::Layer::fillInputInfo(android::Layer::InputDisplayArgs const&)
         Module: file = "C:\Users\Admin\.lldb\module_cache\remote-android\.cache\35EA916F-138D-F7A6-1BA3-B011780073AF\surfaceflinger", arch = "aarch64"
         Symbol: id = {0x000000a4}, range = [0x0000000000160670-0x0000000000161a40), name="android::Layer::fillInputInfo(android::Layer::InputDisplayArgs const&)", mangled="_ZN7android5Layer13fillInputInfoERKNS0_16InputDisplayArgsE"
```
显然可以直接定位到fillInputInfo符号的所指函数，那么寻找此符号引用，可以找到如下伪代码，接着可以大致确定此无符号函数体为上述**部分**源码片段的内联形式。
```cpp
LABEL_19:
    v11 = *(__int64 **)(a1 + 16);
    v12 = 0LL;
    v10 = 0LL;
    v13 = (android::ui::Transform **)&v58;
  }
  else
  {
    v10 = (android::ui::Transform *)(v8 + 72);
    while ( *(_DWORD *)v8 != v6 )
    {
      v8 += 128LL;
      v10 = (android::ui::Transform *)((char *)v10 + 128);
      if ( v8 == v9 )
        goto LABEL_19;
    }
    v12 = *(unsigned __int8 *)(v8 + 113);
    v13 = (android::ui::Transform **)&v60;
    v11 = *(__int64 **)(a1 + 16);
  }
  v60 = v12;
  v58 = 0;
  v14 = *v13;
  v59[0] = v10;
  v59[1] = v14;
  android::Layer::fillInputInfo(v3_layer, v59, (__int64)&v61);
  v16 = v11[1];
  v15 = v11[2];
  if ( v16 >= v15 )
  {
    v29 = 0xAE4C415C9882B9LL;
    v30 = 0x51B3BEA3677D46CFLL * ((__int64)(v16 - *v11) >> 3);
    if ( (unsigned __int64)(v30 + 1) > 0xAE4C415C9882B9LL )
      std::__vector_base_common<true>::__throw_length_error(v11);
```

上述是理想情况，但假如某些代码完全没有符号，也可以尝试借助语义相近的代码或在其它二进制内寻找同义代码以提取特征，并借此进行定位。譬如此句`outWindowInfos.push_back`对应的push_back接口被内联，且在当前二进制内没有符号，但需要注意vector\<T\>::push_back是一个模板函数，虽然无法找到outWindowInfos所属类型的模板函数实例化后的符号，但其它类型的模板函数的符号可能依然存在，如下命令所示。
```
(lldb) image lookup -r -v -n push_back
        Address: surfaceflinger[0x000000000021c60c] (surfaceflinger.PT_LOAD[1]..text + 882188)
        Summary: surfaceflinger`void std::__1::vector<unsigned int, std::__1::allocator<unsigned int>>::__push_back_slow_path<unsigned int const&>(unsigned int const&)
         Module: file = "C:\Users\Admin\.lldb\module_cache\remote-android\.cache\35EA916F-138D-F7A6-1BA3-B011780073AF\surfaceflinger", arch = "aarch64"
         Symbol: id = {0x0000512c}, range = [0x0000005d05cad60c-0x0000005d05cad700), name="void std::__1::vector<unsigned int, std::__1::allocator<unsigned int>>::__push_back_slow_path<unsigned int const&>(unsigned int const&)", mangled="_ZNSt3__16vectorIjNS_9allocatorIjEEE21__push_back_slow_pathIRKjEEvOT_"
```
假定当前进程的二进制文件为surfaceflinger，可以看到LLDB匹配到了此二进制内vector\<unsigned int\>::push_back接口对应的符号，根据如下伪代码可知push_back的实现包含一些特征调用，例如`std::__vector_base_common<true>::__throw_length_error`，那么可以根据这些特征调用反向验证之前匹配到的伪代码片段是否存在push_back接口的调用，以此来进一步判断是否有分析的价值。   
vector\<unsigned int\>::push_back伪代码如下
```cpp
void __fastcall std::vector<unsigned int>::__push_back_slow_path<unsigned int const&>(__int64 a1, _DWORD *a2)
{
...

  v2 = *(void **)a1;
  v4 = *(_QWORD *)(a1 + 8) - *(_QWORD *)a1;
  v5 = (v4 >> 2) + 1;
  if ( v5 >> 62 )
  {
    std::__vector_base_common<true>::__throw_length_error(a1);
LABEL_14:
    operator delete(v2);
    return;
  }
...
```

outWindowInfos所属对象为WindowInfo，当然，我们无法在当前的二进制文件内匹配到对应的模板函数符号，但AOSP的其它部分或许也有调用vector\<WindowInfo\>::push_back的代码，那么使用如下命令在LLDB中尝试匹配。
```
(lldb) image lookup -r -v -n WindowInfo.+push_back
1 match found in C:\Users\Admin\.lldb\module_cache\remote-android\.cache\EA7500B8-6406-B008-D4DA-07426B603D8A\libgui.so:
        Address: libgui.so[0x000000000015d3b8] (gnu_debugdata..text + 873400)
        Summary: libgui.so`void std::__1::vector<android::gui::WindowInfo, std::__1::allocator<android::gui::WindowInfo>>::__push_back_slow_path<android::gui::WindowInfo>(android::gui::WindowInfo&&)
         Module: file = "C:\Users\Admin\.lldb\module_cache\remote-android\.cache\EA7500B8-6406-B008-D4DA-07426B603D8A\libgui.so", arch = "aarch64"
         Symbol: id = {0x00000839}, range = [0x000000000015d3b8-0x000000000015d648), name="void std::__1::vector<android::gui::WindowInfo, std::__1::allocator<android::gui::WindowInfo>>::__push_back_slow_path<android::gui::WindowInfo>(android::gui::WindowInfo&&)", mangled="_ZNSt3__16vectorIN7android3gui10WindowInfoENS_9allocatorIS3_EEE21__push_back_slow_pathIS3_EEvOT_"
```
可以看到的确在libgui.so内匹配到了对应的符号，那么查看此符号对应地址下指令的伪代码形式。
```cpp
void __fastcall std::vector<android::gui::WindowInfo>::__push_back_slow_path<android::gui::WindowInfo>(
        char **a1,
        __int64 a2)
{
...

  v3 = 0x51B3BEA3677D46CFLL * ((a1[1] - *a1) >> 3);
  v4 = v3 + 1;
  if ( (unsigned __int64)(v3 + 1) > 0xAE4C415C9882B9LL )
  {
    v36 = std::__vector_base_common<true>::__throw_length_error(a1);
    sub_15D648(v36);
  }
  else
  {
    if ( 0xA3677D46CEFA8D9ELL * ((a1[2] - *a1) >> 3) > v4 )
      v4 = 0xA3677D46CEFA8D9ELL * ((a1[2] - *a1) >> 3);
    if ( (unsigned __int64)(0x51B3BEA3677D46CFLL * ((a1[2] - *a1) >> 3)) >= 0x572620AE4C415CLL )
      v6 = 0xAE4C415C9882B9LL;
    else
      v6 = v4;
    if ( v6 )
    {
      if ( v6 > 0xAE4C415C9882B9LL )
```
通过伪代码可以看见，代码特征可谓是相当明显，这个模板函数带有很多类似魔数的常量，它们通常用于优化或特定的逻辑处理，回到之前定位到的伪代码片段，通过这些魔数与特征，可以很明显地匹配到几乎一模一样的代码逻辑，那么就完全可以确定vector\<WindowInfo\>::push_back接口在函数体内联后的具体位置，同样，也就可以获取outWindowInfos成员，进行其它的调试操作。

