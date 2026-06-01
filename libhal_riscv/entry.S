#include <nux/hal_config.h>
#include "internal.h"

#define SAVE_REG(_i, _r) sd _r , -((_i) * 8)(sp)
#define RESTORE_REG(_i, _r) ld _r, -((_i) * 8)(a0)


	/* Early interrupt: no return from this. Can be coarse. */
	.globl _early_intrentry
	.align 8
_early_intrentry:
	/* Always in kernel mode. */
	SAVE_REG(1, x1) /* RA */
	SAVE_REG(2, x2) /* SP */
	SAVE_REG(3, x3) /* GP */
	SAVE_REG(4, x4) /* TP */
	SAVE_REG(5, x5)
	SAVE_REG(6, x6)
	SAVE_REG(7, x7)
	SAVE_REG(8, x8)
	SAVE_REG(9, x9)
	SAVE_REG(10, x10)
	SAVE_REG(11, x11)
	SAVE_REG(12, x12)
	SAVE_REG(13, x13)
	SAVE_REG(14, x14)
	SAVE_REG(15, x15)
	SAVE_REG(16, x16)
	SAVE_REG(17, x17)
	SAVE_REG(18, x18)
	SAVE_REG(19, x19)
	SAVE_REG(20, x20)
	SAVE_REG(21, x21)
	SAVE_REG(22, x22)
	SAVE_REG(23, x23)
	SAVE_REG(24, x24)
	SAVE_REG(25, x25)
	SAVE_REG(26, x26)
	SAVE_REG(27, x27)
	SAVE_REG(28, x28)
	SAVE_REG(29, x29)
	SAVE_REG(30, x30)
	SAVE_REG(31, x31)
	csrr t0, sepc
	SAVE_REG(32, t0)
	csrr t0, scause
	SAVE_REG(33, t0)
	csrr t0, stval
	SAVE_REG(34, t0)
	csrr t0, sie
	SAVE_REG(35, t0)
	csrr t0, sstatus
	SAVE_REG(36, t0)

	addi sp, sp, -36*8
	mv a0, sp
	j _hal_entry
	

/* Frame-controlled SSTATUS bits. */
#define SSTATUS_FMASK (SSTATUS_SPIE | SSTATUS_SPP)
	
#define TPOFF_INTRSP 0
#define TPOFF_KERNSP  8	
	
	/* Interrupt handler at steady state, when the cpu is fully setup. */
	.globl _intr_entry
	.align 8
_intr_entry:
	/* Note: SSTATUS SIE set to off. */
	csrrw tp, sscratch, tp
	/* If zero, we're in kernel. */
	bnez tp, 1f
	/* In kernel: switch back. */
	csrrw tp, sscratch, tp
	sd sp, TPOFF_INTRSP(tp)

	SAVE_REG(1, x1) /* RA */
	SAVE_REG(2, x2) /* SP */
	SAVE_REG(3, x3) /* GP */
	SAVE_REG(4, x4) /* TP */
	j 2f

1:	/* In user */
	sd sp, TPOFF_INTRSP(tp)
	ld sp, TPOFF_KERNSP(tp)
	SAVE_REG(1, x1) /* RA */
	ld x1, TPOFF_INTRSP(tp)
	SAVE_REG(2, x1) /* SP */
	SAVE_REG(3, x3) /* GP */
	csrr x1, sscratch
	SAVE_REG(4, x1) /* TP */
	mv x1, x0
	csrrw x0, sscratch, x0

2:
	SAVE_REG(5, x5)
	SAVE_REG(6, x6)
	SAVE_REG(7, x7)
	SAVE_REG(8, x8)
	SAVE_REG(9, x9)
	SAVE_REG(10, x10)
	SAVE_REG(11, x11)
	SAVE_REG(12, x12)
	SAVE_REG(13, x13)
	SAVE_REG(14, x14)
	SAVE_REG(15, x15)
	SAVE_REG(16, x16)
	SAVE_REG(17, x17)
	SAVE_REG(18, x18)
	SAVE_REG(19, x19)
	SAVE_REG(20, x20)
	SAVE_REG(21, x21)
	SAVE_REG(22, x22)
	SAVE_REG(23, x23)
	SAVE_REG(24, x24)
	SAVE_REG(25, x25)
	SAVE_REG(26, x26)
	SAVE_REG(27, x27)
	SAVE_REG(28, x28)
	SAVE_REG(29, x29)
	SAVE_REG(30, x30)
	SAVE_REG(31, x31)
	csrr t0, sepc
	SAVE_REG(32, t0)
	csrr t0, scause
	SAVE_REG(33, t0)
	csrr t0, stval
	SAVE_REG(34, t0)
	csrr t0, sie
	SAVE_REG(35, t0)
	csrr t0, sstatus
	SAVE_REG(36, t0)
	addi sp, sp, -36*8

	mv a0, sp
	call _hal_entry
	
	addi a0, a0, 36*8
	/* SSTATUS: only SPIE/SPP are frame-controlled for sret. */
	RESTORE_REG(36, t0)
	li t1, SSTATUS_FMASK
	and t2, t0, t1
	csrr t0, sstatus
	xori t1, t1, -1
	and t0, t0, t1
	or t0, t0, t2
	csrw sstatus, t0
	/* SIE */
	RESTORE_REG(35, t0)
	csrw sie, t0
	/* Skip STVAL */
	/* Skip SCAUSE */
	RESTORE_REG(32, t0)
	csrw sepc, t0
	RESTORE_REG(31, x31)
	RESTORE_REG(30, x30)
	RESTORE_REG(29, x29)
	RESTORE_REG(28, x28)
	RESTORE_REG(27, x27)
	RESTORE_REG(26, x26)
	RESTORE_REG(25, x25)
	RESTORE_REG(24, x24)
	RESTORE_REG(23, x23)
	RESTORE_REG(22, x22)
	RESTORE_REG(21, x21)
	RESTORE_REG(20, x20)
	RESTORE_REG(19, x19)
	RESTORE_REG(18, x18)
	RESTORE_REG(17, x17)
	RESTORE_REG(16, x16)
	RESTORE_REG(15, x15)
	RESTORE_REG(14, x14)
	RESTORE_REG(13, x13)
	RESTORE_REG(12, x12)
	RESTORE_REG(11, x11)
	/* Skip a0, we're using it. */
	RESTORE_REG(9, x9)
	RESTORE_REG(8, x8)
	RESTORE_REG(7, x7)
	RESTORE_REG(6, x6)
	RESTORE_REG(5, x5)
	RESTORE_REG(4, x4)
	RESTORE_REG(3, x3)
	RESTORE_REG(2, x2)
	RESTORE_REG(1, x1)
	/* Restore A0 now. */
	RESTORE_REG(10, x10)	
	sret


	.globl set_stvec_final
set_stvec_final:
	la t0, _intr_entry
	csrw stvec, t0
	ret
