/*-
 * Copyright (c) 2014 Andrew Turner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/arm64/include/asm.h 280364 2015-03-23 11:54:56Z andrew $
 */

#ifndef _MACHINE_ASM_H_
#define	_MACHINE_ASM_H_

#undef __FBSDID
#if !defined(lint) && !defined(STRIP_FBSDID)
#define	__FBSDID(s)     .ident s
#else
#define	__FBSDID(s)     /* nothing */
#endif

#define	_C_LABEL(x)	x

#define	ENTRY(sym)						\
	.text; .globl sym; .align 2; sym:
#define	EENTRY(sym)						\
	.globl	sym; sym:
#define	END(sym) .size sym, . - sym
#define	EEND(sym)

#define	WEAK_REFERENCE(sym, alias)				\
	.weak alias;						\
	.set alias,sym

//#define	UINT64_C(x)	(x)

#if defined(PIC)
#define	PIC_SYM(x,y)	x ## @ ## y
#else
#define	PIC_SYM(x,y)	x
#endif

/*
 * Sets the trap fault handler. The exception handler will return to the
 * address in the handler register on a data abort or the xzr register to
 * clear the handler. The tmp parameter should be a register able to hold
 * the temporary data.
 */
#define	SET_FAULT_HANDLER(handler, tmp)					\
	ld	tmp, PC_CURTHREAD(gp);		/* Load curthread */	\
	ld	tmp, TD_PCB(tmp);		/* Load the pcb */	\
	sd	handler, PCB_ONFAULT(tmp)	/* Set the handler */

#if 0
#define	SET_FAULT_HANDLER(handler, tmp)					\
	ldr	tmp, [x18, #PC_CURTHREAD];	/* Load curthread */	\
	ldr	tmp, [tmp, #TD_PCB];		/* Load the pcb */	\
	str	handler, [tmp, #PCB_ONFAULT]	/* Set the handler */
#endif /* if 0 */

#define CSR_ZIMM(val) \
	(__builtin_constant_p(val) && ((unsigned long)(val) < 0x20))

#define csr_swap(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	if (CSR_ZIMM(__v)) { 					\
		__asm__ __volatile__ (				\
			"csrrw %0, " #csr ", %1"		\
			: "=r" (__v) : "i" (__v));		\
	} else {						\
		__asm__ __volatile__ (				\
			"csrrw %0, " #csr ", %1"		\
			: "=r" (__v) : "r" (__v));		\
	}							\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	if (CSR_ZIMM(__v)) {					\
		__asm__ __volatile__ (				\
			"csrw " #csr ", %0" : : "i" (__v));	\
	} else {						\
		__asm__ __volatile__ (				\
			"csrw " #csr ", %0" : : "r" (__v));	\
	}							\
})

#define csr_set(csr,val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	if (CSR_ZIMM(__v)) {					\
		__asm__ __volatile__ (				\
			"csrs " #csr ", %0" : : "i" (__v));	\
	} else {						\
		__asm__ __volatile__ (				\
			"csrs " #csr ", %0" : : "r" (__v));	\
	}							\
})

#define csr_clear(csr,val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	if (CSR_ZIMM(__v)) {					\
		__asm__ __volatile__ (				\
			"csrc " #csr ", %0" : : "i" (__v));	\
	} else {						\
		__asm__ __volatile__ (				\
			"csrc " #csr ", %0" : : "r" (__v));	\
	}							\
})

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ (					\
		"csrr %0, " #csr : "=r" (__v));			\
	__v;							\
})

#endif /* _MACHINE_ASM_H_ */
