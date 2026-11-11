/*
 * 控制台输入输出，走 uart。
 * 读操作按行读取。
 * 实现以下特殊输入字符：
 *   换行 -- 行结束
 *   control-h -- 退格
 *   control-u -- 删掉整行
 *   control-d -- 文件结束
 *   control-p -- 打印进程列表
 */
#include "kernel/lib/def.h"
#include "kernel/mm/def.h"
#include "kernel/proc/process.h"

#include "uart.h"

#define BACKSPACE 0x100       // 抹掉上一个已输出字符
#define C(x) ((x) - '@')      // 控制字符 x
#define INPUT_BUF_SIZE 128

/*
 * 往 uart 发一个字符，但不走中断也不 sleep()。
 * 可以安全地在中断里调用，比如 printk 和回显输入字符时。
 */
void console_putc(int c) {
    if (c == BACKSPACE) {
        // 用户敲了退格，就用空格覆盖掉
        uart_putc_sync('\b');
        uart_putc_sync(' ');
        uart_putc_sync('\b');
    } else {
        uart_putc_sync(c);
    }
}

typedef struct {
    char buf[INPUT_BUF_SIZE];
    u32 r; // 读索引
    u32 w; // 写索引
    u32 e; // 编辑索引
} Console;

Console cons;

/*
 *  用户或内核对控制台发起 write() 系统调用时使用
 *  需要转换用户地址为内核地址，并且保证跨页秒调用情况
 *  使用 sleep() 和 UART 中断。
 *  args:
 *      scr 需要输出的内容地址
 *      n 输出长度
 *  return:
 *      int 输出了多少字节
 */
int console_write(void* src, int n) {
    char buf[32];
    int i = 0;

    while (i < n) {
        int nn = sizeof(buf);
        if (nn > n - i) nn = n - i;
        if (memcpy(buf, src, nn) != nn) break;
        uart_write(buf, nn);
        i += nn;
    }

    return i;
}

/*
 *  用户对控制台发起 read() 时走这里。
 *  把（最多）一整行输入拷到 dst。
 *  需要转换用户地址为内核地址，并且保证跨页秒调用情况
 *  args:
 *      scr 需要输入的内容地址
 *      n 输入长度
 *  return:
 *      int 输入了多少字节
 *
 */
int console_read(void* dst, int n) {
    u32 target;
    int c;
    char cbuf;

    target = n;
    while (n > 0) {
        while (cons.r == cons.w) {
            if (current_process->is_killed) {
                return -1;
            }

            sleep_prepare(&cons.r);
            sleep();
        }

        c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

        if (c == C('D')) { // 文件结束
            if (n < target) {
                cons.r--;
            }
            break;
        }

        // 把输入字节拷进用户空间缓冲
        cbuf = c;
        if (memcpy(dst, &cbuf, 1) != 1) break;

        dst++;
        --n;

        if (c == '\n') break;
    }

    return target - n;
}

/*
 * 控制台输入中断处理程序。
 * uartintr() 每读到一个输入字符就调一次。
 * 负责删除/整行删除的处理，追加到 cons.buf，
 * 一整行到齐后唤醒 console_read()。
 */
void console_intr(int c) {
    switch (c) {
        case C('P'): // 打印进程列表
            process_display(current_process);
            break;
        case C('U'): // 删掉整行
            while (cons.e != cons.w &&
             cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
                cons.e--;
                console_putc(BACKSPACE);
            }
            break;
        case C('H'):  // 退格
        case '\x7f':  // Delete 键
            if (cons.e != cons.w) {
                cons.e--;
                console_putc(BACKSPACE);
            }
            break;
        default:
            if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
                c = (c == '\r') ? '\n' : c;

                // 回显给用户
                console_putc(c);

                // 存起来，等 console_read() 消费
                cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

                if (c == '\n' || c == C('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
                    // 一整行（或文件结束）到齐了， 唤醒 console_read()
                    cons.w = cons.e;
                    wakeup(&cons.r);
                }
            }
            break;
    }
}

void console_init(void) {
    uart_init();
}
