#include "uart.h"
#include "console.h"

#include "kernel/proc/lock.h"
#include "kernel/proc/process.h"

#define Reg(reg)            ((volatile unsigned char *)(UART0 + (reg)))
#define ReadReg(reg)        (*(Reg(reg)))
#define WriteReg(reg, v)    (*(Reg(reg)) = (v))

/*
 * UART 的控制寄存器。
 * 部分寄存器读和写含义不同。
 * 详见 http://byterunner.com/16550.html
 */
#define RHR             0        /* 接收保持寄存器（读入的字节） */
#define THR             0        /* 发送保持寄存器（写出的字节） */
#define IER             1        /* 中断使能寄存器 */
#define IER_RX_ENABLE   (1 << 0) /* 接收中断 */
#define IER_TX_ENABLE   (1 << 1) /* 发送中断 */
#define FCR             2        /* FIFO 控制寄存器 */
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR  (3 << 1) /* 清空两个 FIFO 的内容 */
#define ISR             2        /* 中断状态寄存器 */
#define LCR             3        /* 线路控制寄存器 */
#define LCR_EIGHT_BITS  (3 << 0)
#define LCR_BAUD_LATCH  (1 << 7) /* 设置波特率的特殊模式 */
#define LSR             5        /* 线路状态寄存器 */
#define LSR_RX_READY    (1 << 0) /* RHR 中有待读入的字节 */
#define LSR_TX_IDLE     (1 << 5) /* THR 可以再接收一个待发送字节 */

static int tx_chan; /* &tx_chan 就是发送的等待通道 */

void uart_init(void) {
    /*
     * 先关中断
     */
    WriteReg(IER, 0x00);

    /*
     * 进入设置波特率的特殊模式
     */
    WriteReg(LCR, LCR_BAUD_LATCH);

    /*
     * 波特率 38.4K 的低字节
     */
    WriteReg(0, 0x03);

    /*
     * 波特率 38.4K 的高字节
     */
    WriteReg(1, 0x00);

    /*
     * 退出设置波特率模式，
     * 字长 8 位，无校验
     */
    WriteReg(LCR, LCR_EIGHT_BITS);

    /*
     * 复位并使能 FIFO
     */
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    /*
     * 打开发送和接收中断
     */
    WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);
}

/*
 * 把 buf[] 送到 uart。uart 忙时会阻塞，
 * 只能在 write 系统调用里使用
 */
void uart_write(char* buf, int n){
    int i = 0;
    while (i < n) {
        sleep_prepare(&tx_chan);
        if (ReadReg(LSR) & LSR_TX_IDLE) {
            WriteReg(THR, buf[i]);
            i += 1;
        } else {
            sleep();
        }
    }
}

/*
 * 不走中断直接往 uart 写一个字节，
 * 供内核 printk() 和回显使用。会自旋等待
 * uart 的发送寄存器空出来
 */
void uart_putc_sync(int c){
    critical_enter();

    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
    WriteReg(THR, c);

    critical_exit();
}

/*
 * 试着从 UART 读一个输入字符。
 * 没有字符就返回 -1
 */
static int uart_get_char(void) {
    if (ReadReg(LSR) & LSR_RX_READY) {
        return ReadReg(RHR);
    } else {
        return -1;
    }
}

/*
 * 处理 uart 中断：可能有输入到达，
 * 也可能 uart 准备好接下一个输出了，或者两者都有。
 * 由 devintr() 调用
 */
void uart_intr(void) {
    ReadReg(ISR); /* 应答中断 */

    if (ReadReg(LSR) & LSR_TX_IDLE) {
        wakeup(&tx_chan);
    }

    while (1) {
        int c = uart_get_char();
        if (c == -1) break;
        console_intr(c);
    }
}
