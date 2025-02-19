Auth: MG193.7
# bindings/gumjs/gumv8stalker.cpp
---
## *gumjs_stalker_follow*
  - 调用 `gum_stalker_follow`

# gum/backend-x86/gumstalker-x86.c
---
## *gum_stalker_follow*
  - 调用 `gum_process_modify_thread(thread_id, func = gum_stalker_infect)`

## *gum_stalker_infect*
  - 调用 `gum_stalker_create_exec_ctx`
    - 调用 `gum_exec_ctx_new`
      - 初始化 stalker 参数和 code_writer
  - 使用 GUM_CPU_CONTEXT_XIP 获取当前PC
  - ctx->current_block = `gum_exec_ctx_obtain_block_for(ctx, PC, code_address)`
    - 如果 ctx->mappings 为空，则调用 `gum_exec_ctx_build_block`
      - block = `gum_exec_block_new(ctx)`
        - 初始化 stalker 和 GumExecBlock，用于保存指令
      - 调用 `gum_exec_ctx_compile_block(ctx, block, PC)`
      - 调用 `gum_exec_block_commit` 以更新 block 指令代码
      - 通过 `gum_metal_hash_table_insert` 插入当前块信息到 ctx->mappings
    - 持续收集代码块直到输入结束，并调整代码块
  - 调用 `gum_exec_ctx_maybe_unfollow` 检查是否继续跟踪
  - 调用 gum_exec_ctx_write_ 序列函数来生成插桩后的代码块
  - 所有插桩操作完成后，设置 cpu_context->rip 指向插桩代码块
    - 这将最终使感染的线程控制流劫持到 stalker 空间

## *gum_exec_ctx_compile_block*
  - 调用 `gum_x86_relocator_reset(rl, PC, code_writer)`
    - rl->input_start = PC
    - rl->input_cur = PC
    - rl->input_pc = PC
  - 调用 `gum_ensure_code_readable(rl, stalker_page_size)`
    - 对地址进行页对齐，确保当前PC周围的每个页面权限为RWX
  - GumGeneratorContext gc.relocator = rl
  - GumStalkerIterator iterator.generator_context = gc
  - ctx->transform_block_impl(ctx->transformer, &iterator, &output)
    - 调用 `gum_default_stalker_transformer_transform_block`
      - while(gum_stalker_iterator_next(iterator, NULL))
      - `gum_stalker_iterator_keep(iterator)`，如果 iterator 到达块的末尾（eob），则跳出循环，否则保持 while 循环

## *gum_stalker_iterator_next*
  - 如果 iterator 到达块的末尾（eob），返回 FALSE
  - `gum_x86_relocator_read_one(rl, rl->gc->instruction->ci)`
  - 更新 rl->gc->instruction 的开始和结束地址

# gum/arch-x86/gumx86relocator.c
---
## *gum_x86_relocator_read_one*
  - cs_malloc # 这是 capstone API，用于初始化单个指令的处理器
  - `cs_disasm_iter(capstone, PC)` # capstone API，从 PC 地址读取指令，读取后更新 PC 地址为下一个指令
  - 更新 relocator 的 input_cur 和 input_pc
  - 返回当前指令的大小

# gum/backend-linux/gumprocess-linux.c
---
## *gum_process_modify_thread*
  - ctx.func = `gum_stalker_infect`
  - 调用 `gum_linux_modify_thread(thread_id, func = gum_do_modify_thread, ctx)`

## *gum_linux_modify_thread*
  - ctx_2.func = `gum_do_modify_thread`
  - ctx_2.user_data = ctx
  - `socketpair(ctx_2.fd)` 初始化 IPC 文件描述符
  - 调用 glibc `clone(fn = gum_linux_do_modify_thread, ctx_2)` 创建子进程
  - prctl PR_SET_DUMPABLE 设为 1
  - prctl PR_SET_PTRACER 设置子进程可以进行自我 ptrace
  - 调用 `gum_linux_handle_modify_thread_comms`
  - prctl PR_SET_DUMPABLE 设为 0
  - 等待子进程 waitpid

## *gum_linux_do_modify_thread*
  - 调用 `gum_libc_ptrace` 对特定的 thread_id 进行 ptrace
  - 使用 ptrace PEEKUSER 保存当前线程的 CPU 上下文
  - 在存储当前 CPU 上下文后，通过 ctx.fd 发送 GUM_ACK_READ_REGISTERS
    - 这将通知 `gum_linux_handle_modify_thread_comms` 调用 ctx->func
  - 等待 GUM_ACK_MODIFIED_REGISTERS，如果收到，则通过 ptrace POKEUSER 设置调整后的 &ctx 回到 CPU

## *gum_linux_handle_modify_thread_comms*
  - 等待 GUM_ACK_READ_REGISTERS
  - 调用 ctx_2->func(thread_id, 当前 CPU 上下文, ctx_2->user_data)
    - ctx_2->func == `gum_do_modify_thread`
  - 通过 ctx.fd 发送 GUM_ACK_MODIFIED_REGISTERS

## *gum_do_modify_thread*
  - `gum_parse_gp_regs` 解析当前 CPU 上下文
  - ctx_2->user_data->func(thread_id, cpu_context)
    - ctx_2->user_data->func = `gum_stalker_infect`

