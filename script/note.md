
`llvm-objdump -d build/bin/kernel.elf` 反汇编

`llvm-addr2line -e build/bin/kernel.elf 8020015e` 找到指定内存的具体代码位置


