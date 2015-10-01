/* LINTLIBRARY */
/*-
 * Copyright 1996-1998 John D. Polstra.
 * Copyright 2014 Andrew Turner.
 * Copyright 2014-2015 The FreeBSD Foundation.
 * All rights reserved.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/csu/aarch64/crt1.c 285044 2015-07-02 15:02:59Z andrew $");

#ifndef lint
#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif
#endif /* lint */

#include <stdlib.h>

#include "libc_private.h"
#include "crtbrand.c"
#include "ignore_init.c"

#ifdef GCRT
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

extern long * _end;

void __start(int, char **, char **, void (*)(void));

/* The entry function. */
__asm("	.text			\n"
"	.align	0		\n"
"	.globl	_start		\n"
"	_start:			\n"
/* TODO: Remove this when the kernel correctly aligns the stack */
"	bnez	a0, 1f		\n" /* Are we using a new kernel? */
"	mv	a0, sp		\n" /* No, load the args from sp */
"	and	sp, a0, ~0xf	\n" /* And align the stack */
"1:	mv	a3, a2		\n" /* cleanup */
"	addi	a1, a0, 8	\n" /* load argv */
"	ld	a0, 0(a0)	\n" /* load argc */
"	slli	t0, a0, 3	\n" /* mult by 8 */
"	add	a2, a1, t0	\n" /* env is after argv */
"	addi	a2, a2, 8	\n" /* argv is null terminated */
"	la	gp, _gp		\n" /* load global pointer */
"	call	 __start  ");

/* The entry function. */
void
__start(int argc, char *argv[], char *env[], void (*cleanup)(void))
{

	handle_argv(argc, argv, env);

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else {
		/*
		 * Hack to resolve _end so we read the correct symbol.
		 * Without this it will resolve to the copy in the library
		 * that firsts requests it. We should fix the toolchain,
		 * however this is is needed until this can take place.
		 */
		*(volatile long *)&_end;

		_init_tls();
	}

#ifdef GCRT
	atexit(_mcleanup);
	monstartup(&eprol, &etext);
__asm__("eprol:");
#endif

	handle_static_init(argc, argv, env);
	exit(main(argc, argv, env));
}
