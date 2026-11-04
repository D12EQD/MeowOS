# MeowOS 项目约定

RISC-V64 OS，OpenSBI 引导，单核 SV39，单核无 SMP

目录：`src/kernel/`（内核）、`src/user/`（用户态）、`build/`（产物）。

## 命名

- 结构体：`PascalCase` — `Process`, `TrapFrame`, `File`, `Inode`, `FileOP`
- 函数：`snake_case` — `create_user_process`, `find_ready_process`, `process_prepare_return_user`
- 全局变量：`snake_case` — `current_process`, `process_list`, `boot_info`
- 宏 / 枚举成员：`SCREAMING_SNAKE_CASE` — `PROCESS_MAX_COUNT`, `TRAPFRAME_VADDR`, `SYS_COUNT`
- 类型别名：`_t` 后缀（`pt_t`）或 `u8/u16/u32/u64/i64/size_t` 前缀
- 调试函数：`xxx_debug` — `process_debug`, `trap_frame_debug`

## 格式

- 4 空格缩进，不用 tab
- 指针星号靠变量：`Process *p`，**不写** `Process* p`
- 控制语句关键字后留空格：`if (x)`, `while (1)`, `for (int i = 0; ...)`, `return (u64)-1`
- 函数名后不留空格：`foo()`，但定义时 `void foo(void)` 或 `void foo()` 都行（跟随同文件已用的风格）
- K&R 大括号：控制结构 `{` 同行，函数定义 `{` 下一行
- 一行一语句

## 注释

- **中文注释**
- 单行用 `//`，多行/块用 `/* */`
- 非显然的分支、寄存器偏移、魔数必须注释
- 反例：不要写 `/* 保存寄存器 */` 这种把指令复述一遍的注释

## 类型

- 只用 `kernel/lib/typedef.h` 里的 `u8/u16/u32/u64/i64/size_t`，**不直接写** `unsigned long` / `uint64_t`
- 返回值有错误可能时用 `i64`；成功返回 0/true/指针，失败返回 `(u64)-1` / false / NULL
- 结构体字段顺序：先大字段后小字段（节省 padding），但可读性优先

## 错误处理

- 不变量用 `ASSERT(cond)` 保护，不要在 ASSERT 之后假设它能通过
- 致命错误用 `PANIC("func: 什么错, 关键值 = %lx", val)`，**消息带上下文**
- 多步资源分配失败用 `goto` 统一清理，标签按「反序释放」排列：
  ```c
  pt_t page_table = user_alloc_page_table();
  if (!page_table) goto page_table_free;
  if (!mappages(page_table, ...)) goto trampoline_free;
  ...
  trampoline_free:
      user_unmap_pages(page_table, TRAMPOLINE_VADDR, 1, false);
  page_table_free:
      if (page_table) free_walk(page_table, 2);
      return NULL;

## 其他介绍
1. 不是传统的构建系统，该构建系统使用 nob ，构建脚本放在 `build/src` 下


