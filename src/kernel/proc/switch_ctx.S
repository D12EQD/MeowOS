.globl switch_ctx

# switch_ctx (ThreadCtx *old, ThreadCtx* new));
switch_ctx:
    # 将所有通用寄存器保存在 old context 中
    # 保存返回地址，下次再调用时回到 old 的执行流中
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)
    sd ra, 0(a0)
    sd sp, 8(a0)
    
    # 加载所有 new context 的内容
    # 更新返回地址，当前函数返回的时候直接执行 new 对于的 ra 返回地址
    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)
    ret
