# Soot Dev Note
20250723  
## Project Structure
- analyze
  - handler 多功能句柄处理模块，单个句柄能串联多个阶段
    - BaseHandler 句柄基类
    - CallGraphHandler 调用图生成句柄
  - phase 多阶段调控模块
    - PhaseBaseHandler 调控基类
    - CallGraphPhase 调用图生成
  - utils 工具模块
    - OptionsUtils.java：参数初始化/Options调整
## TODO List
[ ] 生成全程序调用图  
[ ] 调用图序列化存储  
