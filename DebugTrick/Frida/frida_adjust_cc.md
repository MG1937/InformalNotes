# Frida适配调用约定
`new NativeFunction(address, returnType, argTypes[, abi])`  
Frida的官方文档内NativeFunction, 支持手动指定调用约定, 即abi可选项. 但有时会遇到一些不在支持范围内的调用约定, 这里给出的方案是通过Interceptor.replace强行替换目标函数, 并设置中转NativeCallback, 在中转函数内手动操作寄存器或内存来适配调用约定.  

该方案由Frida作者Oleavr提供, issue请参考:  
https://github.com/frida/frida/issues/1629  

示例代码如下
```
var getDesc = Module.findExportByName("libinputflinger.so","_ZNK7android15inputdispatcher11MotionEntry14getDescriptionEv");
var getDesc_func = new NativeFunction(getDesc, 'pointer', ['pointer','pointer']);

var wrapper = new NativeCallback(function (thisPtr, a2, a3) {
    console.log("wrapper >> " + a2);
    this.context.x8 = a2; // 通过replace可以获取到this.context上下文
    console.log("replaced >> " + JSON.stringify(this.context));
    getDesc_func(thisPtr, a2, a2); // 调用原函数
    // NativeCallback更多参数与操作详见frida文档.
}, 'void', ['pointer', 'pointer', 'pointer']);

Interceptor.replace(getDesc,wrapper);
Interceptor.flush();
```
随后手动调用getDesc_func可以触发被替换的函数, 可以在替换的回调内操作CPU上下文, 或者手动开栈填入参数, 这里只给出手动填入x8寄存器的例子.
