#include "plic.h"


// the riscv Platform Level Interrupt Controller (PLIC).
//

void plic_init(void){
    // set desired IRQ priorities non-zero (otherwise disabled).
    *(u32 *)(PLIC + UART0_IRQ * 4) = 1;
    *(u32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
    plic_init_hart();
}

void plic_init_hart(void){
    int hart = 0;

    // set enable bits for this hart's S-mode
    // for the uart and virtio disk.
    *(u32 *)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

    // set this hart's S-mode priority threshold to 0.
    *(u32 *)PLIC_SPRIORITY(hart) = 0;
}

/* ask the PLIC what interrupt we should serve. */
int plic_claim(void) {
    int hart = 0;
    int irq = *(u32 *)PLIC_SCLAIM(hart);
    return irq;
}

/* tell the PLIC we've served this IRQ. */
void plic_complete(int irq) {
    int hart = 0;
    *(u32 *)PLIC_SCLAIM(hart) = irq;
}
