.section .user_image, "a"
.globl _binary_user_bin_start
.globl _binary_user_bin_end

_binary_user_bin_start:
    .incbin "build/bin/user.bin"
_binary_user_bin_end:
