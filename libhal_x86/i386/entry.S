/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <machine/asm.h>
#include "i386.h"
#include "../internal.h"

	.altmacro
.macro _clac_user_frame csoff
	/* Clear user-carried AC before C; leave kernel user-copy faults alone. */
	mov %cr4, %eax
	test $CR4_SMAP, %eax
	jz 1f
	testw $3, \csoff(%esp)
	jz 1f
	.byte 0x0f, 0x01, 0xca
1:
.endm

.macro _do_entry vct
	pushal
	mov  %cr3, %eax
	push %eax
	mov %cr2, %eax
	push %eax
	pushw %gs
	pushw %fs
	pushw %es
	pushw %ds
	_clac_user_frame 56
	mov $KDS, %ax
	mov %ax, %ds
	mov %ax, %es
	call _C_LABEL(_i386_fs)
	mov %ax, %fs
	xor %ax, %ax
	mov %ax, %gs
	mov %esp, %eax
	pushl %eax

	.if \vct == VECT_SYSC
	call _C_LABEL(do_syscall)
	.else
	pushl $\vct
	.if \vct == 2
	call _C_LABEL(do_nmi)
	.elseif \vct < 32
	call _C_LABEL(do_xcpt)
	.else
	call _C_LABEL(do_vect)
	.endif
	.endif

	mov %eax, %esp
.endm

.macro _do_iret_exit
	popw %ds
	popw %es
	popw %fs
	popw %gs
	/* Skip CR2 and CR3 */
	addl $8, %esp
	popal
	/* Skip error code */
	add $4, %esp
	iret
.endm

.macro handler_e vct
	.globl hdlr_&\vct
hdlr_&\vct&:
	_do_entry \vct
	_do_iret_exit
.endm

.macro handler vct
	.globl hdlr_&\vct
hdlr_&\vct&:
	pushl $0
	_do_entry \vct
	_do_iret_exit
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

ENTRY(___usrentry_enter)
	push %esp
	mov %esp, %ebp
	mov 8(%ebp), %esp
	_do_iret_exit
END(___usrentry_enter)


