/*-
 * Copyright (c) 2013 Andrew Turner <andrew@freebsd.org>
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
 * $FreeBSD: head/sys/arm64/include/atomic.h 284196 2015-06-09 23:54:20Z zbb $
 */

#ifndef	_MACHINE_ATOMIC_H_
#define	_MACHINE_ATOMIC_H_

#define	isb()		__asm __volatile("isb" : : : "memory")

/*
 * Options for DMB and DSB:
 *	oshld	Outer Shareable, load
 *	oshst	Outer Shareable, store
 *	osh	Outer Shareable, all
 *	nshld	Non-shareable, load
 *	nshst	Non-shareable, store
 *	nsh	Non-shareable, all
 *	ishld	Inner Shareable, load
 *	ishst	Inner Shareable, store
 *	ish	Inner Shareable, all
 *	ld	Full system, load
 *	st	Full system, store
 *	sy	Full system, all
 */
//#define	dsb(opt)	__asm __volatile("dsb " __STRING(opt) : : : "memory")
//#define	dmb(opt)	__asm __volatile("dmb " __STRING(opt) : : : "memory")

#define	dsb(opt)	__asm __volatile("")
#define	dmb(opt)	__asm __volatile("")

#define	mb()	dmb(sy)	/* Full system memory barrier all */
#define	wmb()	dmb(st)	/* Full system memory barrier store */
#define	rmb()	dmb(ld)	/* Full system memory barrier load */

static __inline void
atomic_add_32(volatile uint32_t *p, uint32_t val)
{

	__asm __volatile("amoadd.w zero, %1, %0"
			: "+A" (p)
			: "r" (val));

#if 0
	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   add1	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

static __inline void
atomic_clear_32(volatile uint32_t *p, uint32_t val)
{
	//uint32_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   bic8	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

static __inline int
atomic_cmpset_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	//uint32_t tmp;
	int res;

	res = 0;

#if 0
	__asm __volatile(
	    "1: mov	%w1, #1        \n"
	    "   aaa \n"
	    "   ldxr	%w0, [%2]      \n"
	    "   cmp	%w0, %w3       \n"
	    "   b.ne	2f             \n"
	    "   stxr	%w1, %w4, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (cmpval), "+r" (newval)
	    : : "cc"
	);
#endif

	return (!res);
}

static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t ret;

	__asm __volatile("amoadd.w %0, %2, %1"
			: "=r" (ret), "+A" (p)
			: "r" (val));

#if 0
	__asm __volatile(
	    "1: ldxr	%w4, [%2]      \n"
	    "   add22	%w0, %w4, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val), "=&r"(ret) : : "cc"
	);
#endif

	return (ret);
}

static __inline uint32_t
atomic_readandclear_32(volatile uint32_t *p)
{
	uint32_t ret;
	//uint32_t tmp, ret;
	//int res;

	ret = 0;

#if 0
	__asm __volatile(
	    "   mov	%w0, #0        \n"
	    "1: ldxr	%w3, [%2]      \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "=&r"(ret) : : "cc"
	);
#endif

	return (ret);
}

static __inline void
atomic_set_32(volatile uint32_t *p, uint32_t val)
{
	//uint32_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   orr	%w0, %w0, %w3  \n"
	    "   stxr2	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

static __inline void
atomic_subtract_32(volatile uint32_t *p, uint32_t val)
{
	//uint32_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   sub4	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

#define	atomic_add_int		atomic_add_32
#define	atomic_clear_int	atomic_clear_32
#define	atomic_cmpset_int	atomic_cmpset_32
#define	atomic_fetchadd_int	atomic_fetchadd_32
#define	atomic_readandclear_int	atomic_readandclear_32
#define	atomic_set_int		atomic_set_32
#define	atomic_subtract_int	atomic_subtract_32

static __inline void
atomic_add_acq_32(volatile uint32_t *p, uint32_t val)
{
	//uint32_t tmp;
	//int res;

	__asm __volatile("amoadd.w.aq zero, %1, %0"
			: "+A" (p)
			: "r" (val));

#if 0
	__asm __volatile(
	    "1: ldaxr	%w0, [%2]      \n"
	    "   add	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
#endif
}

static __inline void
atomic_clear_acq_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t tmp;
	int res;

	__asm __volatile(
	    "1: ldaxr	%w0, [%2]      \n"
	    "   bic	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline int
atomic_cmpset_acq_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	//uint32_t tmp;
	int res;

	res = 0;

#if 0
	__asm __volatile(
	    "1: mov	%w1, #1        \n"
	    "   ldaxr	%w0, [%2]      \n"
	    "   cmp	%w0, %w3       \n"
	    "   b.ne	2f             \n"
	    "   stxr	%w1, %w4, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (cmpval), "+r" (newval)
	    : : "cc", "memory"
	);
#endif

	return (!res);
}

static __inline uint32_t
atomic_load_acq_32(volatile uint32_t *p)
{
	uint32_t ret;

	ret = 0;
#if 0
	__asm __volatile(
	    "ldar	%w0, [%1] \n"
	    : "=&r" (ret) : "r" (p) : "memory");
#endif

	return (ret);
}

static __inline void
atomic_set_acq_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t tmp;
	int res;

	__asm __volatile(
	    "1: ldaxr	%w0, [%2]      \n"
	    "   orr	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline void
atomic_subtract_acq_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t tmp;
	int res;

	__asm __volatile(
	    "1: ldaxr	%w0, [%2]      \n"
	    "   sub	%w0, %w0, %w3  \n"
	    "   stxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

#define	atomic_add_acq_int	atomic_add_acq_32
#define	atomic_clear_acq_int	atomic_clear_acq_32
#define	atomic_cmpset_acq_int	atomic_cmpset_acq_32
#define	atomic_load_acq_int	atomic_load_acq_32
#define	atomic_set_acq_int	atomic_set_acq_32
#define	atomic_subtract_acq_int	atomic_subtract_acq_32

/* The atomic functions currently are both acq and rel, we should fix this. */

static __inline void
atomic_add_rel_32(volatile uint32_t *p, uint32_t val)
{
	//uint32_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   add	%w0, %w0, %w3  \n"
	    "   stlxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
#endif
}

static __inline void
atomic_clear_rel_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t tmp;
	int res;

	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   bic	%w0, %w0, %w3  \n"
	    "   stlxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline int
atomic_cmpset_rel_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	//uint32_t tmp;
	int res;

	res = 0;

#if 0
	__asm __volatile(
	    "1: mov	%w1, #1        \n"
	    "   ldxr	%w0, [%2]      \n"
	    "   cmp	%w0, %w3       \n"
	    "   b.ne	2f             \n"
	    "   stlxr	%w1, %w4, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (cmpval), "+r" (newval)
	    : : "cc", "memory"
	);
#endif

	return (!res);
}

static __inline void
atomic_set_rel_32(volatile uint32_t *p, uint32_t val)
{
	uint32_t tmp;
	int res;

	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   orr	%w0, %w0, %w3  \n"
	    "   stlxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline void
atomic_store_rel_32(volatile uint32_t *p, uint32_t val)
{

#if 0
	__asm __volatile(
	    "stlr	%w0, [%1] \n"
	    : : "r" (val), "r" (p) : "memory");
#endif
}

static __inline void
atomic_subtract_rel_32(volatile uint32_t *p, uint32_t val)
{
	//uint32_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%w0, [%2]      \n"
	    "   sub	%w0, %w0, %w3  \n"
	    "   stlxr	%w1, %w0, [%2] \n"
            "   cbnz	%w1, 1b        \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
#endif

}

#define	atomic_add_rel_int	atomic_add_rel_32
#define	atomic_clear_rel_int	atomic_add_rel_32
#define	atomic_cmpset_rel_int	atomic_cmpset_rel_32
#define	atomic_set_rel_int	atomic_set_rel_32
#define	atomic_subtract_rel_int	atomic_subtract_rel_32
#define	atomic_store_rel_int	atomic_store_rel_32


static __inline void
atomic_add_64(volatile uint64_t *p, uint64_t val)
{

	__asm __volatile("amoadd.d zero, %1, %0"
			: "+A" (p)
			: "r" (val));

#if 0
	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   add4	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r" (tmp), "=&r" (res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

static __inline void
atomic_clear_64(volatile uint64_t *p, uint64_t val)
{
	//uint64_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   bic	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

static __inline int
atomic_cmpset_64(volatile uint64_t *p, uint64_t cmpval, uint64_t newval)
{
	uint64_t res;
	uint32_t tmp;

	__asm __volatile(
		"1: li      %1, 1          \n"
		"   lr.d    %0, (%2)       \n"
		"   bne     %0, %3, 2f     \n"
		"   sc.w.aq %1, %4, (%2)   \n"
		"   bnez    %1, 1b         \n"
		"2:"
		: "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (cmpval), "+r" (newval)
		: : "cc", "memory"
	);

#if 0
	__asm __volatile(
	    "1: mov	%w1, #1       \n"
	    "   ldxr	%0, [%2]      \n"
	    "   cmp	%0, %3        \n"
	    "   b.ne	2f            \n"
	    "   stxr	%w1, %4, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    "2:"
	    : "=&r" (tmp), "=&r"(res), "+r" (p), "+r" (cmpval), "+r" (newval)
	    : : "cc", "memory"
	);
#endif

	return (!res);
}

static __inline uint64_t
atomic_fetchadd_64(volatile uint64_t *p, uint64_t val)
{
	//uint64_t tmp, ret;
	//int res;

	uint64_t ret;

	__asm __volatile("amoadd.d %0, %2, %1"
			: "=r" (ret), "+A" (p)
			: "r" (val));

#if 0
	__asm __volatile(
	    "1: ldxr	%4, [%2]      \n"
	    "   add45	%0, %4, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val), "=&r"(ret) : : "cc"
	);
#endif

	return (ret);
}

static __inline uint64_t
atomic_readandclear_64(volatile uint64_t *p)
{
	uint64_t tmp, ret;
	int res;

	__asm __volatile(
	    "   mov	%0, #0        \n"
	    "1: ldxr	%3, [%2]      \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "=&r"(ret) : : "cc"
	);

	return (ret);
}

static __inline void
atomic_set_64(volatile uint64_t *p, uint64_t val)
{
	//uint64_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   orr5	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif
}

static __inline void
atomic_subtract_64(volatile uint64_t *p, uint64_t val)
{
	//uint64_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   sub9	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc"
	);
#endif

}

static __inline uint64_t
atomic_swap_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t old;
	//int res;

	old = 0;
#if 0
	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   stxr1	%w1, %3, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(old), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
#endif

	return (old);
}

#define	atomic_add_long			atomic_add_64
#define	atomic_clear_long		atomic_clear_64
#define	atomic_cmpset_long		atomic_cmpset_64
#define	atomic_fetchadd_long		atomic_fetchadd_64
#define	atomic_readandclear_long	atomic_readandclear_64
#define	atomic_set_long			atomic_set_64
#define	atomic_subtract_long		atomic_subtract_64

#define	atomic_add_ptr			atomic_add_64
#define	atomic_clear_ptr		atomic_clear_64
#define	atomic_cmpset_ptr		atomic_cmpset_64
#define	atomic_fetchadd_ptr		atomic_fetchadd_64
#define	atomic_readandclear_ptr		atomic_readandclear_64
#define	atomic_set_ptr			atomic_set_64
#define	atomic_subtract_ptr		atomic_subtract_64

static __inline void
atomic_add_acq_64(volatile uint64_t *p, uint64_t val)
{
	//uint64_t tmp;
	//int res;

	__asm __volatile("amoadd.d.aq zero, %1, %0"
			: "+A" (p)
			: "r" (val));

#if 0
	__asm __volatile(
	    "1: ldaxr	%0, [%2]      \n"
	    "   add	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
#endif
}

static __inline void
atomic_clear_acq_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	int res;

	__asm __volatile(
	    "1: ldaxr	%0, [%2]      \n"
	    "   bic	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline int
atomic_cmpset_acq_64(volatile uint64_t *p, uint64_t cmpval, uint64_t newval)
{
	//uint64_t tmp;
	int res;

	res = 0;

#if 0
	__asm __volatile(
	    "1: mov	%w1, #1       \n"
	    "   ldaxr	%0, [%2]      \n"
	    "   cmp	%0, %3        \n"
	    "   b.ne	2f            \n"
	    "   stxr	%w1, %4, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    "2:"
	    : "=&r" (tmp), "=&r" (res), "+r" (p), "+r" (cmpval), "+r" (newval)
	    : : "cc", "memory"
	);
#endif

	return (!res);
}

static __inline uint64_t
atomic_load_acq_64(volatile uint64_t *p)
{
	uint64_t ret;

	__asm __volatile(
	    "ldar	%0, [%1] \n"
	    : "=&r" (ret) : "r" (p) : "memory");

	return (ret);
}

static __inline void
atomic_set_acq_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	int res;

	__asm __volatile(
	    "1: ldaxr	%0, [%2]      \n"
	    "   orr	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline void
atomic_subtract_acq_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	int res;

	__asm __volatile(
	    "1: ldaxr	%0, [%2]      \n"
	    "   sub	%0, %0, %3    \n"
	    "   stxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

#define	atomic_add_acq_long		atomic_add_acq_64
#define	atomic_clear_acq_long		atomic_add_acq_64
#define	atomic_cmpset_acq_long		atomic_cmpset_acq_64
#define	atomic_load_acq_long		atomic_load_acq_64
#define	atomic_set_acq_long		atomic_set_acq_64
#define	atomic_subtract_acq_long	atomic_subtract_acq_64

#define	atomic_add_acq_ptr		atomic_add_acq_64
#define	atomic_clear_acq_ptr		atomic_add_acq_64
#define	atomic_cmpset_acq_ptr		atomic_cmpset_acq_64
#define	atomic_load_acq_ptr		atomic_load_acq_64
#define	atomic_set_acq_ptr		atomic_set_acq_64
#define	atomic_subtract_acq_ptr		atomic_subtract_acq_64

/*
 * TODO: The atomic functions currently are both acq and rel, we should fix
 * this.
 */

static __inline void
atomic_add_rel_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	int res;

	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   add	%0, %0, %3    \n"
	    "   stlxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    "2:"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline void
atomic_clear_rel_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	int res;

	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   bic	%0, %0, %3    \n"
	    "   stlxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline int
atomic_cmpset_rel_64(volatile uint64_t *p, uint64_t cmpval, uint64_t newval)
{
	//uint64_t tmp;
	int res;

	res = 0;

#if 0
	__asm __volatile(
	    "1: mov	%w1, #1       \n"
	    "   ldxr	%0, [%2]      \n"
	    "   cmp	%0, %3        \n"
	    "   b.ne	2f            \n"
	    "   stlxr	%w1, %4, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    "2:"
	    : "=&r" (tmp), "=&r" (res), "+r" (p), "+r" (cmpval), "+r" (newval)
	    : : "cc", "memory"
	);
#endif

	return (!res);
}

static __inline void
atomic_set_rel_64(volatile uint64_t *p, uint64_t val)
{
	uint64_t tmp;
	int res;

	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   orr	%0, %0, %3    \n"
	    "   stlxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
}

static __inline void
atomic_store_rel_64(volatile uint64_t *p, uint64_t val)
{

#if 0
	__asm __volatile(
	    "stlr	%0, [%1] \n"
	    : : "r" (val), "r" (p) : "memory");
#endif
}

static __inline void
atomic_subtract_rel_64(volatile uint64_t *p, uint64_t val)
{
	//uint64_t tmp;
	//int res;

#if 0
	__asm __volatile(
	    "1: ldxr	%0, [%2]      \n"
	    "   sub	%0, %0, %3    \n"
	    "   stlxr	%w1, %0, [%2] \n"
            "   cbnz	%w1, 1b       \n"
	    : "=&r"(tmp), "=&r"(res), "+r" (p), "+r" (val) : : "cc", "memory"
	);
#endif
}

static __inline void
atomic_thread_fence_acq(void)
{

	//dmb(ld);
}

static __inline void
atomic_thread_fence_rel(void)
{

	//dmb(sy);
}

static __inline void
atomic_thread_fence_acq_rel(void)
{

	//dmb(sy);
}

static __inline void
atomic_thread_fence_seq_cst(void)
{

	//dmb(sy);
}

#define	atomic_add_rel_long		atomic_add_rel_64
#define	atomic_clear_rel_long		atomic_clear_rel_64

#define	atomic_add_rel_long		atomic_add_rel_64
#define	atomic_clear_rel_long		atomic_clear_rel_64
#define	atomic_cmpset_rel_long		atomic_cmpset_rel_64
#define	atomic_set_rel_long		atomic_set_rel_64
#define	atomic_subtract_rel_long	atomic_subtract_rel_64
#define	atomic_store_rel_long		atomic_store_rel_64

#define	atomic_add_rel_ptr		atomic_add_rel_64
#define	atomic_clear_rel_ptr		atomic_clear_rel_64
#define	atomic_cmpset_rel_ptr		atomic_cmpset_rel_64
#define	atomic_set_rel_ptr		atomic_set_rel_64
#define	atomic_subtract_rel_ptr		atomic_subtract_rel_64
#define	atomic_store_rel_ptr		atomic_store_rel_64

#endif /* _MACHINE_ATOMIC_H_ */

