# Parcel 结构快速解析
// TODO: 详细内容待补充   

配合binder-trace提取有效数据   

Dalvik Debug + 表达式辅助解析   
    1. 对照原序列化流程, 逐步read以移动pos
    2. 遇到难以解析的结构时, 借用对应对象的writeToParcel直接移动pos, 跳过复杂结构

Native   
    1. 部分aidl的编译产物若cs无法找到, 直接逆向二进制   
    2. writeParcelable会在开头readInt导致pos移动, 若跳过此步会有解析偏差
