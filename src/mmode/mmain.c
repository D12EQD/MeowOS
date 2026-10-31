#include "mmode/def.h"
#include "mmode/mmain.h"

// enable device interrupts and return the old value
static inline u64 interrupt_on() {
    u64 res = READ_SET_CSR(CSR_SSTATUS, MSTATUS_SIE);
    return res;
}

// disable device interrupts and return the old value
static inline u64 interrupt_off() {
    u64 res = READ_CLEAN_CSR(CSR_SSTATUS, MSTATUS_SIE);
    return res;
}

// is interrupt ?
static inline bool is_enable_interrupt(u64 *ret){
    u64 x = READ_CSR(CSR_SSTATUS);
    if (ret) *ret = x;
    return (x & MSTATUS_SIE) == 0;
}

static inline bool is_disable_interrupt(u64 *ret){
    u64 x = READ_CSR(CSR_SSTATUS);
    if (ret) *ret = x;
    return (x & MSTATUS_SIE) != 0;
}

struct sbi_scratch {
	/** Start (or base) address of firmware linked to OpenSBI library */
	unsigned long fw_start;
	/** Size (in bytes) of firmware linked to OpenSBI library */
	unsigned long fw_size;
	/** Offset (in bytes) of the R/W section */
	unsigned long fw_rw_offset;
	/** Offset (in bytes) of the heap area */
	unsigned long fw_heap_offset;
	/** Size (in bytes) of the heap area */
	unsigned long fw_heap_size;
	/** Arg1 (or 'a1' register) of next booting stage for this HART */
	unsigned long next_arg1;
	/** Address of next booting stage for this HART */
	unsigned long next_addr;
	/** Privilege mode of next booting stage for this HART */
	unsigned long next_mode;
	/** Warm boot entry point address for this HART */
	unsigned long warmboot_addr;
	/** Address of sbi_platform */
	unsigned long platform_addr;
	/** Address of HART ID to sbi_scratch conversion function */
	unsigned long hartid_to_scratch;
	/** Address of current trap context */
	unsigned long trap_context;
	/** Temporary storage */
	unsigned long tmp0;
	/** Temporary storage */
	unsigned long tmp1;
	/** Options for OpenSBI library */
	unsigned long options;
	/** Index of the hart */
	unsigned long hartindex;
};

EXTERN_FUNC(void sbi_init(struct sbi_scratch *scratch));

struct sbi_scratch my_scratch;

void mmain(){
    // * 1. The 'mscratch' CSR is pointing to sbi_scratch of current HART
    // unsigned long hartid = 0; // NOTE: 单核所以设置为0
    my_scratch.fw_start  = 0x80000000UL; 
    my_scratch.fw_size   = 0x00150000UL;   

    // 设置下一阶段（你的 S-mode 内核）的信息
    my_scratch.next_mode = PRV_S;
    my_scratch.next_addr = 0x80200000UL;
    my_scratch.next_arg1 = 0; // TODO : 传递设备树

    WRITE_CSR(CSR_MSCRATCH, &my_scratch);
    // * 2. Stack pointer (SP) is setup for current HART 
    // 在汇编中设置完成

    // * 3. Interrupts are disabled in MSTATUS CSR
    CLEAN_CSR(CSR_MSTATUS, 1 << 3);

    // * 4. All interrupts are disabled in MIE CSR
    WRITE_CSR(CSR_MIE, 0);

    sbi_init(&my_scratch);
    while (1);
}
