# 常用命令记录
sxe ld:xx.sys  
    - 加载某模块时挂起

db 0x123456 L10
    - 以byte为单位查看某地址10个单位长度的内存, 若以word则使用dw, 以此类推

s -u 0 L?ffffffff "TestDriver Hello"
    - s命令搜索某范围的字符串, -a代表以ascii格式搜索, -u则表示unicode格式
