

# 1. 常用指令 

反汇编
```bash
llvm-objdump -d build/bin/kernel.elf > debug.txt 
```
找到指定内存的具体代码位置
```bash
llvm-addr2line -e build/bin/kernel.elf 8020015e
```
# 2. 重点内容 

## 2.1 首个进程执行过程 (xv6 中)

首先需要强调的是`switch_ctx`中，将老线程上下文和新线程上下文切换，直接相当于切换了执行流，不要用典型的c思维想着函数还回来吗。
首次调用进程调度函数的是 `main.c` 中的 `scheduler` 该函数会对所有的进程进行检查，每次时会将进程上锁并且快速解锁; 但是如果找到的话，不会立刻解锁。
`scheduler` 中的 `switch_ctx` 会切换上下文，每个进程的上下文一开始就指向`fork_ret`，切换上下文后执行该函数，并且保存`scheduler`的上下文，下次调用 `switch_ctx` 的时候就再次落入`scheduler`的调度函数中。
回到`fork_ret`，其中最重要的就是三点

1. 解锁进程
2. 首次进程调用会初始化一下文件系统
3. 调用`process_prepare_return_user`做最后的用户内容初始化，然后执行用户的进程

## 2.2

```bash
(qemu) stop
(qemu) info registers          # 看 satp、pc、scause、sepc
(qemu) info mem                # 看页表映射
(qemu) xp /x 0x80253000+512*4  # 看一级页表项
(qemu) xp /1024x 0x80254000    # 看二级页表
(qemu) xp /32b 0x80265000      # 看用户镜像物理页
(qemu) xp /8i 0x80265000       # 反汇编用户镜像
```


