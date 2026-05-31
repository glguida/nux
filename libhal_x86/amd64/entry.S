/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <machine/asm.h>
#include <nux/hal_config_amd64.h>
#include "amd64.h"
#include "../internal.h"

.macro _clac_user_frame csoff
	/* Clear user-carried AC before C; leave kernel user-copy faults alone. */
	mov %cr4, %rax
	test $CR4_SMAP, %rax
	jz 1f
	testq $3, \csoff(%rsp)
	jz 1f
	.byte 0x0f, 0x01, 0xca
1:
.endm

	.globl _syscall_frame_entry
_syscall_frame_entry:
	/* Setup RSP, then emulate IRET frame. */
	swapgs
	movq %rsp, %gs:16 /* Save rsp in scratch. */
	movq %gs:8, %rsp  /* Restore kernel rsp. */

	pushq $UDS
	pushq %gs:16
	push %r11
	pushq $UCS
	push %rcx
	pushq $0 /* Error code. */

	pushq $0 /* Vector */

	push %r15
	push %r14
	push %r13
	push %r12
	pushq $0
	push %r10
	push %r9
	push %r8
	push %rdi
	push %rsi
	push %rbp
	push %rdx
	pushq $0
	push %rbx
	push %rax
	mov %cr2, %rax
	push %rax

	swapgs
	/* Save GSBASE. There are better ways. */
	mov $MSR_IA32_GS_BASE, %rcx
	rdmsr
	shl $32, %rdx
	or %rdx, %rax
	push %rax

	/* Save FSBASE. There are better ways. */
	mov $MSR_IA32_FS_BASE, %rcx
	rdmsr
	shl $32, %rdx
	or %rdx, %rax
	push %rax

	pushq $FRAMETYPE_SYSC
	_clac_user_frame 176

	mov %rsp, %rdi

	/* Restore Kernel GS BASE unconditionally. */
	mov $MSR_IA32_KERNEL_GS_BASE, %rcx
	rdmsr
	mov $MSR_IA32_GS_BASE, %rcx
	wrmsr

	call _C_LABEL(do_syscall_entry)
	mov %rax, %rsp

	// switch to IRET if frame type is intr
	mov $FRAMETYPE_INTR, %rax
	cmpq (%rsp), %rax
	je _intr_frame_return

	add $8, %rsp /* Skip frame type: syscall frame */

	/* Restore FSBASE */
	popq %rax
	mov %rax, %rdx
	shr $32, %rdx
	mov $MSR_IA32_FS_BASE, %rcx
	wrmsr

	/* Restore GSBASE */
	popq %rax
	mov %rax, %rdx
	shr $32, %rdx
	mov $MSR_IA32_GS_BASE, %rcx
	wrmsr

	add $8, %rsp /* Skip CR2 */
	popq %rax
	popq %rbx
	xor %rcx, %rcx /* clear RCX */
	add $8, %rsp
	popq %rdx
	popq %rbp
	popq %rsi
	popq %rdi
	popq %r8
	popq %r9
	popq %r10
	xor %r11, %r11 /* clear RCX */
	add $8, %rsp
	popq %r12
	popq %r13
	popq %r14
	popq %r15
	add $16, %rsp /* Skip vect, error */
	/* Get SYSRET from iret stack. */
	pop %rcx /* EIP */
	add $8, %rsp /* Skip CS */
	pop %r11 /* FLAGS */
	pop %rsp /* RSP */
	/* SS Ignored. RSP has changed. */
	rex.w sysret

	.altmacro
.macro _do_entry vct
	pushq $\vct

	push %r15
	push %r14
	push %r13
	push %r12
	push %r11
	push %r10
	push %r9
	push %r8
	push %rdi
	push %rsi
	push %rbp
	push %rdx
	push %rcx
	push %rbx
	push %rax
	mov %cr2, %rax
	push %rax

	/* Save GS BASE. There are better ways. */
	mov $MSR_IA32_GS_BASE, %rcx
	rdmsr
	shl $32, %rdx
	or %rdx, %rax
	push %rax

	mov $MSR_IA32_FS_BASE, %rcx
	rdmsr
	shl $32, %rdx
	or %rdx, %rax
	push %rax

	pushq $FRAMETYPE_INTR
	_clac_user_frame 176
	
	mov %rsp, %rdi

	/* Restore Kernel GS BASE unconditionally. */
	mov $MSR_IA32_KERNEL_GS_BASE, %rcx
	rdmsr
	mov $MSR_IA32_GS_BASE, %rcx
	wrmsr

	call _C_LABEL(do_intr_entry)
	mov %rax, %rsp

	jmp _intr_frame_return
.endm

_intr_frame_return:

	add $8, %rsp /* Skip frame type: interrupt frame */

	/* Restore FSBASE */
	popq %rax
	mov %rax, %rdx
	shr $32, %rdx
	mov $MSR_IA32_FS_BASE, %rcx
	wrmsr

	/* Restore GSBASE */
	popq %rax
	mov %rax, %rdx
	shr $32, %rdx
	mov $MSR_IA32_GS_BASE, %rcx
	wrmsr

	add $8, %rsp /* Skip CR2 */
	popq %rax
	popq %rbx
	popq %rcx
	popq %rdx
	popq %rbp
	popq %rsi
	popq %rdi
	popq %r8
	popq %r9
	popq %r10
	popq %r11
	popq %r12
	popq %r13
	popq %r14
	popq %r15
	add $16, %rsp /* Skip vect, error */
	iretq


.macro handler_e vct
	.globl hdlr_&\vct
hdlr_&\vct&:
	_do_entry \vct
.endm

.macro handler vct
	.globl hdlr_&\vct
hdlr_&\vct&:
	pushq $0
	_do_entry \vct
.endm
	

.macro repeat macro from to
	\macro \from
	.if \to-\from
	repeat \macro,%(\from+1),\to
	.endif
.endm

	repeat handler 0, 7
	handler 8
	handler 9
	repeat handler_e 10, 14
	handler 15
	handler 16
	handler_e 17
	repeat handler 18, 31
	repeat handler 32, 63
	repeat handler 64, 127
	repeat handler 128, 191
	repeat handler 192, 255
