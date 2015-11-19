/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory as part of the CTSRD Project, with support from the UK Higher
 * Education Innovation Fund (HEIF).
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
 */

/*
 * RISC-V Interrupt Controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/smp.h>
#include <machine/asm.h>
#include <machine/trap.h>

#include <arm64/arm64/gic.h>

#include "pic_if.h"

/* We are using GICv2 register naming */

/* Distributor Registers */
#define GICD_CTLR		0x000			/* v1 ICDDCR */
#define GICD_TYPER		0x004			/* v1 ICDICTR */
#define GICD_IIDR		0x008			/* v1 ICDIIDR */
#define GICD_IGROUPR(n)		(0x0080 + ((n) * 4))	/* v1 ICDISER */
#define GICD_ISENABLER(n)	(0x0100 + ((n) * 4))	/* v1 ICDISER */
#define GICD_ICENABLER(n)	(0x0180 + ((n) * 4))	/* v1 ICDICER */
#define GICD_ISPENDR(n)		(0x0200 + ((n) * 4))	/* v1 ICDISPR */
#define GICD_ICPENDR(n)		(0x0280 + ((n) * 4))	/* v1 ICDICPR */
#define GICD_ICACTIVER(n)	(0x0380 + ((n) * 4))	/* v1 ICDABR */
#define GICD_IPRIORITYR(n)	(0x0400 + ((n) * 4))	/* v1 ICDIPR */
#define GICD_ITARGETSR(n)	(0x0800 + ((n) * 4))	/* v1 ICDIPTR */
#define GICD_ICFGR(n)		(0x0C00 + ((n) * 4))	/* v1 ICDICFR */
#define GICD_SGIR(n)		(0x0F00 + ((n) * 4))	/* v1 ICDSGIR */

/* CPU Registers */
#define GICC_CTLR		0x0000			/* v1 ICCICR */
#define GICC_PMR		0x0004			/* v1 ICCPMR */
#define GICC_BPR		0x0008			/* v1 ICCBPR */
#define GICC_IAR		0x000C			/* v1 ICCIAR */
#define GICC_EOIR		0x0010			/* v1 ICCEOIR */
#define GICC_RPR		0x0014			/* v1 ICCRPR */
#define GICC_HPPIR		0x0018			/* v1 ICCHPIR */
#define GICC_ABPR		0x001C			/* v1 ICCABPR */
#define GICC_IIDR		0x00FC			/* v1 ICCIIDR*/

#define	GIC_FIRST_IPI		 0	/* Irqs 0-15 are SGIs/IPIs. */
#define	GIC_LAST_IPI		15
#define	GIC_FIRST_PPI		16	/* Irqs 16-31 are private (per */
#define	GIC_LAST_PPI		31	/* core) peripheral interrupts. */
#define	GIC_FIRST_SPI		32	/* Irqs 32+ are shared peripherals. */

/* First bit is a polarity bit (0 - low, 1 - high) */
#define GICD_ICFGR_POL_LOW	(0 << 0)
#define GICD_ICFGR_POL_HIGH	(1 << 0)
#define GICD_ICFGR_POL_MASK	0x1
/* Second bit is a trigger bit (0 - level, 1 - edge) */
#define GICD_ICFGR_TRIG_LVL	(0 << 1)
#define GICD_ICFGR_TRIG_EDGE	(1 << 1)
#define GICD_ICFGR_TRIG_MASK	0x2

//#define	NR_IRQS		2
//#define	IRQ_SOFTWARE	0
//#define	IRQ_TIMER	1

enum {
	IRQ_SOFTWARE,
	IRQ_TIMER,
	NIRQS
};
#define	SR_IE		(1 << 0)

#if 0
static struct resource_spec arm_gic_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* Distributor registers */
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },	/* CPU Interrupt Intf. registers */
	{ -1, 0 }
};
#endif

static struct arm_gic_softc *arm_gic_sc = NULL;

#define	gic_c_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->gic_c_bst, (_sc)->gic_c_bsh, (_reg))
#define	gic_c_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->gic_c_bst, (_sc)->gic_c_bsh, (_reg), (_val))
#define	gic_d_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->gic_d_bst, (_sc)->gic_d_bsh, (_reg))
#define	gic_d_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->gic_d_bst, (_sc)->gic_d_bsh, (_reg), (_val))

static pic_dispatch_t gic_dispatch;
static pic_eoi_t gic_eoi;
static pic_mask_t gic_mask_irq;
static pic_unmask_t gic_unmask_irq;

#ifdef SMP
static void
gic_init_secondary(device_t dev)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->nirqs; i += 4)
		gic_d_write_4(sc, GICD_IPRIORITYR(i >> 2), 0);

	/* Set all the interrupts to be in Group 0 (secure) */
	for (i = 0; i < sc->nirqs; i += 32) {
		gic_d_write_4(sc, GICD_IGROUPR(i >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(sc, GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(sc, GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(sc, GICD_CTLR, 0x01);

	/*
	 * Activate the timer interrupts: virtual, secure, and non-secure.
	 */
	gic_d_write_4(sc, GICD_ISENABLER(27 >> 5), (1UL << (27 & 0x1F)));
	gic_d_write_4(sc, GICD_ISENABLER(29 >> 5), (1UL << (29 & 0x1F)));
	gic_d_write_4(sc, GICD_ISENABLER(30 >> 5), (1UL << (30 & 0x1F)));
}
#endif

static int
arm_gic_attach(device_t dev)
{
	struct		arm_gic_softc *sc;
	//int		i;
	//uint32_t	icciidr;

	if (arm_gic_sc)
		return (ENXIO);

	sc = device_get_softc(dev);

	//if (bus_alloc_resources(dev, arm_gic_spec, sc->gic_res)) {
	//	device_printf(dev, "could not allocate resources\n");
	//	return (ENXIO);
	//}

	sc->gic_dev = dev;
	arm_gic_sc = sc;

	/* Initialize mutex */
	mtx_init(&sc->mutex, "GIC lock", "", MTX_SPIN);

	sc->nirqs = 2;
	arm_register_root_pic(dev, sc->nirqs);

	//csr_set(sstatus, SR_IE);
	//csr_clear(sie, (1 << 5));

#if 0
	/* Distributor Interface */
	sc->gic_d_bst = rman_get_bustag(sc->gic_res[0]);
	sc->gic_d_bsh = rman_get_bushandle(sc->gic_res[0]);

	/* CPU Interface */
	sc->gic_c_bst = rman_get_bustag(sc->gic_res[1]);
	sc->gic_c_bsh = rman_get_bushandle(sc->gic_res[1]);

	/* Disable interrupt forwarding to the CPU interface */
	gic_d_write_4(sc, GICD_CTLR, 0x00);

	/* Get the number of interrupts */
	sc->nirqs = gic_d_read_4(sc, GICD_TYPER);
	sc->nirqs = 32 * ((sc->nirqs & 0x1f) + 1);

	arm_register_root_pic(dev, sc->nirqs);

	icciidr = gic_c_read_4(sc, GICC_IIDR);
	device_printf(dev,"pn 0x%x, arch 0x%x, rev 0x%x, implementer 0x%x irqs %u\n",
			icciidr>>20, (icciidr>>16) & 0xF, (icciidr>>12) & 0xf,
			(icciidr & 0xfff), sc->nirqs);

	/* Set all global interrupts to be level triggered, active low. */
	for (i = 32; i < sc->nirqs; i += 16) {
		gic_d_write_4(sc, GICD_ICFGR(i >> 4), 0x00000000);
	}

	/* Disable all interrupts. */
	for (i = 32; i < sc->nirqs; i += 32) {
		gic_d_write_4(sc, GICD_ICENABLER(i >> 5), 0xFFFFFFFF);
	}

	for (i = 0; i < sc->nirqs; i += 4) {
		gic_d_write_4(sc, GICD_IPRIORITYR(i >> 2), 0);
		gic_d_write_4(sc, GICD_ITARGETSR(i >> 2),
		    1 << 0 | 1 << 8 | 1 << 16 | 1 << 24);
	}

	/* Set all the interrupts to be in Group 0 (secure) */
	for (i = 0; i < sc->nirqs; i += 32) {
		gic_d_write_4(sc, GICD_IGROUPR(i >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(sc, GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(sc, GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(sc, GICD_CTLR, 0x01);
#endif

	return (0);
}

static void gic_dispatch(device_t dev, struct trapframe *frame)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	uint32_t active_irq;
	int first = 1;

	active_irq = (frame->tf_scause & 0xf);
	if (frame->tf_scause & (1 << 31)) {

		//printf("gic_dispatch %d\n", active_irq);
		arm_dispatch_intr(active_irq, frame);
		return;
	}

	return;

	while (1) {
		active_irq = gic_c_read_4(sc, GICC_IAR);

		/*
		 * Immediatly EOIR the SGIs, because doing so requires the other
		 * bits (ie CPU number), not just the IRQ number, and we do not
		 * have this information later.
		 */

		if ((active_irq & 0x3ff) <= GIC_LAST_IPI)
			gic_c_write_4(sc, GICC_EOIR, active_irq);
		active_irq &= 0x3FF;

		if (active_irq == 0x3FF) {
			if (first)
				printf("Spurious interrupt detected\n");
			return;
		}

		arm_dispatch_intr(active_irq, frame);
		first = 0;
	}
}

static void
gic_eoi(device_t dev, u_int irq)
{
	//struct arm_gic_softc *sc = device_get_softc(dev);

	//printf("%s\n", __func__);
	//gic_c_write_4(sc, GICC_EOIR, irq);
}

void
gic_mask_irq(device_t dev, u_int irq)
{
	//struct arm_gic_softc *sc = device_get_softc(dev);

	printf("gic_mask_irq\n");

	//gic_d_write_4(sc, GICD_ICENABLER(irq >> 5), (1UL << (irq & 0x1F)));
	//gic_c_write_4(sc, GICC_EOIR, irq);
}

void
gic_unmask_irq(device_t dev, u_int irq)
{
	struct arm_gic_softc *sc;

	sc = device_get_softc(dev);

	//printf("gic_unmask_irq %d\n", irq);

	switch (irq) {
	case IRQ_TIMER:
		csr_set(sie, SIE_STIE);
		break;
	case IRQ_SOFTWARE:
		//csr_set(sie, SIE_SSIE);
		break;
	default:
		panic("Unknown irq %d\n", irq);
	}

	//panic("%s: %d\n", __func__, irq);
	//printf("%s: %d\n", __func__, irq);
	//gic_d_write_4(sc, GICD_ISENABLER(irq >> 5), (1UL << (irq & 0x1F)));
}

#ifdef SMP
static void
gic_ipi_send(device_t dev, cpuset_t cpus, u_int ipi)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	uint32_t val = 0, i;

	for (i = 0; i < MAXCPU; i++)
		if (CPU_ISSET(i, &cpus))
			val |= 1 << (16 + i);

	gic_d_write_4(sc, GICD_SGIR(0), val | ipi);
}

static int
arm_gic_ipi_read(device_t dev, int i)
{

	if (i != -1) {
		/*
		 * The intr code will automagically give the frame pointer
		 * if the interrupt argument is 0.
		 */
		if ((unsigned int)i > 16)
			return (0);
		return (i);
	}

	return (0x3ff);
}

static void
arm_gic_ipi_clear(device_t dev, int ipi)
{
	/* no-op */
}
#endif

static device_method_t arm_gic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	arm_gic_attach),

	/* pic_if */
	DEVMETHOD(pic_dispatch,		gic_dispatch),
	DEVMETHOD(pic_eoi,		gic_eoi),
	DEVMETHOD(pic_mask,		gic_mask_irq),
	DEVMETHOD(pic_unmask,		gic_unmask_irq),

#ifdef SMP
	DEVMETHOD(pic_init_secondary,	gic_init_secondary),
	DEVMETHOD(pic_ipi_send,		gic_ipi_send),
#endif

	{ 0, 0 }
};

DEFINE_CLASS_0(gic, arm_gic_driver, arm_gic_methods,
    sizeof(struct arm_gic_softc));
