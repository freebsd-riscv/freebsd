/*-
 * Copyright (c) 2008-2012 Semihalf.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include "opt_platform.h"
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/hid.h>
#include <machine/machdep.h>
#include <machine/platform.h>
#include <machine/platformvar.h>
#include <machine/smp.h>
#include <machine/spr.h>
#include <machine/vmparam.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include "platform_if.h"

#ifdef SMP
extern void *ap_pcpu;
extern vm_paddr_t kernload;		/* Kernel physical load address */
extern uint8_t __boot_page[];		/* Boot page body */
extern uint32_t bp_kernload;
#endif

extern uint32_t *bootinfo;
vm_offset_t ccsrbar_va;

static int cpu, maxcpu;

static int mpc85xx_probe(platform_t);
static void mpc85xx_mem_regions(platform_t, struct mem_region *phys,
    int *physsz, struct mem_region *avail, int *availsz);
static u_long mpc85xx_timebase_freq(platform_t, struct cpuref *cpuref);
static int mpc85xx_smp_first_cpu(platform_t, struct cpuref *cpuref);
static int mpc85xx_smp_next_cpu(platform_t, struct cpuref *cpuref);
static int mpc85xx_smp_get_bsp(platform_t, struct cpuref *cpuref);
static int mpc85xx_smp_start_cpu(platform_t, struct pcpu *cpu);
static void mpc85xx_idle(platform_t, int cpu);
static int mpc85xx_idle_wakeup(platform_t plat, int cpu);

static void mpc85xx_reset(platform_t);

static platform_method_t mpc85xx_methods[] = {
	PLATFORMMETHOD(platform_probe,		mpc85xx_probe),
	PLATFORMMETHOD(platform_attach,		mpc85xx_attach),
	PLATFORMMETHOD(platform_mem_regions,	mpc85xx_mem_regions),
	PLATFORMMETHOD(platform_timebase_freq,	mpc85xx_timebase_freq),

	PLATFORMMETHOD(platform_smp_first_cpu,	mpc85xx_smp_first_cpu),
	PLATFORMMETHOD(platform_smp_next_cpu,	mpc85xx_smp_next_cpu),
	PLATFORMMETHOD(platform_smp_get_bsp,	mpc85xx_smp_get_bsp),
	PLATFORMMETHOD(platform_smp_start_cpu,	mpc85xx_smp_start_cpu),

	PLATFORMMETHOD(platform_reset,		mpc85xx_reset),
	PLATFORMMETHOD(platform_idle,		mpc85xx_idle),
	PLATFORMMETHOD(platform_idle_wakeup,	mpc85xx_idle_wakeup),

	PLATFORMMETHOD_END
};

DEFINE_CLASS_0(mpc85xx, mpc85xx_platform, mpc85xx_methods, 0);

PLATFORM_DEF(mpc85xx_platform);

static int
mpc85xx_probe(platform_t plat)
{
	u_int pvr = mfpvr() >> 16;

	if ((pvr & 0xfff0) == FSL_E500v1)
		return (BUS_PROBE_DEFAULT);

	return (ENXIO);
}

int
mpc85xx_attach(platform_t plat)
{
	phandle_t cpus, child, ccsr;
	const char *soc_name_guesses[] = {"/soc", "soc", NULL};
	const char **name;
	pcell_t ranges[6], acells, pacells, scells;
	uint32_t sr;
	uint64_t ccsrbar, ccsrsize;
	int i, law_max, tgt;

	if ((cpus = OF_finddevice("/cpus")) != -1) {
		for (maxcpu = 0, child = OF_child(cpus); child != 0;
		    child = OF_peer(child), maxcpu++)
			;
	} else
		maxcpu = 1;

	/*
	 * Locate CCSR region. Irritatingly, there is no way to find it
	 * unless you already know where it is. Try to infer its location
	 * from the device tree.
	 */

	ccsr = -1;
	for (name = soc_name_guesses; *name != NULL && ccsr == -1; name++)
		ccsr = OF_finddevice(*name);
	if (ccsr == -1) {
		char type[64];

	 	/* That didn't work. Search for devices of type "soc" */
		child = OF_child(OF_peer(0));
		for (OF_child(child); child != 0; child = OF_peer(child)) {
			if (OF_getprop(child, "device_type", type, sizeof(type))
			    <= 0)
				continue;

			if (strcmp(type, "soc") == 0) {
				ccsr = child;
				break;
			}
		}
	}

	if (ccsr == -1)
		panic("Could not locate CCSR window!");

	OF_getprop(ccsr, "#size-cells", &scells, sizeof(scells));
	OF_getprop(ccsr, "#address-cells", &acells, sizeof(acells));
	OF_searchprop(OF_parent(ccsr), "#address-cells", &pacells,
	    sizeof(pacells));
	OF_getprop(ccsr, "ranges", ranges, sizeof(ranges));
	ccsrbar = ccsrsize = 0;
	for (i = acells; i < acells + pacells; i++) {
		ccsrbar <<= 32;
		ccsrbar |= ranges[i];
	}
	for (i = acells + pacells; i < acells + pacells + scells; i++) {
		ccsrsize <<= 32;
		ccsrsize |= ranges[i];
	}
	ccsrbar_va = pmap_early_io_map(ccsrbar, ccsrsize);

	mpc85xx_fix_errata(ccsrbar_va);
	mpc85xx_enable_l3_cache();

	/*
	 * Clear local access windows. Skip DRAM entries, so we don't shoot
	 * ourselves in the foot.
	 */
	law_max = law_getmax();
	for (i = 0; i < law_max; i++) {
		sr = ccsr_read4(OCP85XX_LAWSR(i));
		if ((sr & OCP85XX_ENA_MASK) == 0)
			continue;
		tgt = (sr & 0x01f00000) >> 20;
		if (tgt == OCP85XX_TGTIF_RAM1 || tgt == OCP85XX_TGTIF_RAM2 ||
		    tgt == OCP85XX_TGTIF_RAM_INTL)
			continue;

		ccsr_write4(OCP85XX_LAWSR(i), sr & OCP85XX_DIS_MASK);
	}

	return (0);
}

void
mpc85xx_mem_regions(platform_t plat, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz)
{

	ofw_mem_regions(phys, physsz, avail, availsz);
}

static u_long
mpc85xx_timebase_freq(platform_t plat, struct cpuref *cpuref)
{
	u_long ticks;
	phandle_t cpus, child;
	pcell_t freq;

	if (bootinfo != NULL) {
		if (bootinfo[0] == 1) {
			/* Backward compatibility. See 8-STABLE. */
			ticks = bootinfo[3] >> 3;
		} else {
			/* Compatibility with Juniper's loader. */
			ticks = bootinfo[5] >> 3;
		}
	} else
		ticks = 0;

	if ((cpus = OF_finddevice("/cpus")) == -1)
		goto out;

	if ((child = OF_child(cpus)) == 0)
		goto out;

	switch (OF_getproplen(child, "timebase-frequency")) {
	case 4:
	{
		uint32_t tbase;
		OF_getprop(child, "timebase-frequency", &tbase, sizeof(tbase));
		ticks = tbase;
		return (ticks);
	}
	case 8:
	{
		uint64_t tbase;
		OF_getprop(child, "timebase-frequency", &tbase, sizeof(tbase));
		ticks = tbase;
		return (ticks);
	}
	default:
		break;
	}

	freq = 0;
	if (OF_getprop(child, "bus-frequency", (void *)&freq,
	    sizeof(freq)) <= 0)
		goto out;

	/*
	 * Time Base and Decrementer are updated every 8 CCB bus clocks.
	 * HID0[SEL_TBCLK] = 0
	 */
	if (freq != 0)
#ifdef QORIQ_DPAA
		ticks = freq / 32;
#else
		ticks = freq / 8;
#endif

out:
	if (ticks <= 0)
		panic("Unable to determine timebase frequency!");

	return (ticks);
}

static int
mpc85xx_smp_first_cpu(platform_t plat, struct cpuref *cpuref)
{

	cpu = 0;
	cpuref->cr_cpuid = cpu;
	cpuref->cr_hwref = cpuref->cr_cpuid;
	if (bootverbose)
		printf("powerpc_smp_first_cpu: cpuid %d\n", cpuref->cr_cpuid);
	cpu++;

	return (0);
}

static int
mpc85xx_smp_next_cpu(platform_t plat, struct cpuref *cpuref)
{

	if (cpu >= maxcpu)
		return (ENOENT);

	cpuref->cr_cpuid = cpu++;
	cpuref->cr_hwref = cpuref->cr_cpuid;
	if (bootverbose)
		printf("powerpc_smp_next_cpu: cpuid %d\n", cpuref->cr_cpuid);

	return (0);
}

static int
mpc85xx_smp_get_bsp(platform_t plat, struct cpuref *cpuref)
{

	cpuref->cr_cpuid = mfspr(SPR_PIR);
	cpuref->cr_hwref = cpuref->cr_cpuid;

	return (0);
}

static int
mpc85xx_smp_start_cpu(platform_t plat, struct pcpu *pc)
{
#ifdef SMP
	vm_paddr_t bptr;
	uint32_t reg;
	int timeout;
	uintptr_t brr;
	int cpuid;

#ifdef QORIQ_DPAA
	uint32_t tgt;

	reg = ccsr_read4(OCP85XX_COREDISR);
	cpuid = pc->pc_cpuid;

	if ((reg & cpuid) != 0) {
		printf("%s: CPU %d is disabled!\n", __func__, pc->pc_cpuid);
		return (-1);
	}

	brr = OCP85XX_BRR;
#else /* QORIQ_DPAA */
	brr = OCP85XX_EEBPCR;
	cpuid = pc->pc_cpuid + 24;
#endif
	bp_kernload = kernload;
	reg = ccsr_read4(brr);
	if ((reg & (1 << cpuid)) != 0) {
		printf("SMP: CPU %d already out of hold-off state!\n",
		    pc->pc_cpuid);
		return (ENXIO);
	}

	ap_pcpu = pc;
	__asm __volatile("msync; isync");

	/* Flush caches to have our changes hit DRAM. */
	cpu_flush_dcache(__boot_page, 4096);

	bptr = ((vm_paddr_t)(uintptr_t)__boot_page - KERNBASE) + kernload;
	KASSERT((bptr & 0xfff) == 0,
	    ("%s: boot page is not aligned (%#jx)", __func__, (uintmax_t)bptr));
#ifdef QORIQ_DPAA

	/*
	 * Read DDR controller configuration to select proper BPTR target ID.
	 *
	 * On P5020 bit 29 of DDR1_CS0_CONFIG enables DDR controllers
	 * interleaving. If this bit is set, we have to use
	 * OCP85XX_TGTIF_RAM_INTL as BPTR target ID. On other QorIQ DPAA SoCs,
	 * this bit is reserved and always 0.
	 */

	reg = ccsr_read4(OCP85XX_DDR1_CS0_CONFIG);
	if (reg & (1 << 29))
		tgt = OCP85XX_TGTIF_RAM_INTL;
	else
		tgt = OCP85XX_TGTIF_RAM1;

	/*
	 * Set BSTR to the physical address of the boot page
	 */
	ccsr_write4(OCP85XX_BSTRH, bptr >> 32);
	ccsr_write4(OCP85XX_BSTRL, bptr);
	ccsr_write4(OCP85XX_BSTAR, OCP85XX_ENA_MASK |
	    (tgt << OCP85XX_TRGT_SHIFT) | (ffsl(PAGE_SIZE) - 2));

	/* Read back OCP85XX_BSTAR to synchronize write */
	ccsr_read4(OCP85XX_BSTAR);

	/*
	 * Enable and configure time base on new CPU.
	 */

	/* Set TB clock source to platform clock / 32 */
	reg = ccsr_read4(CCSR_CTBCKSELR);
	ccsr_write4(CCSR_CTBCKSELR, reg & ~(1 << pc->pc_cpuid));

	/* Enable TB */
	reg = ccsr_read4(CCSR_CTBENR);
	ccsr_write4(CCSR_CTBENR, reg | (1 << pc->pc_cpuid));
#else

	/*
	 * Set BPTR to the physical address of the boot page
	 */
	bptr = (bptr >> 12) | 0x80000000u;
	ccsr_write4(OCP85XX_BPTR, bptr);
	__asm __volatile("isync; msync");

#endif /* QORIQ_DPAA */

	/*
	 * Release AP from hold-off state
	 */
	reg = ccsr_read4(brr);
	ccsr_write4(brr, reg | (1 << cpuid));
	__asm __volatile("isync; msync");

	timeout = 500;
	while (!pc->pc_awake && timeout--)
		DELAY(1000);	/* wait 1ms */

	/*
	 * Disable boot page translation so that the 4K page at the default
	 * address (= 0xfffff000) isn't permanently remapped and thus not
	 * usable otherwise.
	 */
#ifdef QORIQ_DPAA
	ccsr_write4(OCP85XX_BSTAR, 0);
#else
	ccsr_write4(OCP85XX_BPTR, 0);
#endif
	__asm __volatile("isync; msync");

	if (!pc->pc_awake)
		printf("SMP: CPU %d didn't wake up.\n", pc->pc_cpuid);
	return ((pc->pc_awake) ? 0 : EBUSY);
#else
	/* No SMP support */
	return (ENXIO);
#endif
}

static void
mpc85xx_reset(platform_t plat)
{

	/*
	 * Try the dedicated reset register first.
	 * If the SoC doesn't have one, we'll fall
	 * back to using the debug control register.
	 */
	ccsr_write4(OCP85XX_RSTCR, 2);

	/* Clear DBCR0, disables debug interrupts and events. */
	mtspr(SPR_DBCR0, 0);
	__asm __volatile("isync");

	/* Enable Debug Interrupts in MSR. */
	mtmsr(mfmsr() | PSL_DE);

	/* Enable debug interrupts and issue reset. */
	mtspr(SPR_DBCR0, mfspr(SPR_DBCR0) | DBCR0_IDM | DBCR0_RST_SYSTEM);

	printf("Reset failed...\n");
	while (1)
		;
}

static void
mpc85xx_idle(platform_t plat, int cpu)
{
#ifdef QORIQ_DPAA
	uint32_t reg;

	reg = ccsr_read4(OCP85XX_RCPM_CDOZCR);
	ccsr_write4(OCP85XX_RCPM_CDOZCR, reg | (1 << cpu));
	ccsr_read4(OCP85XX_RCPM_CDOZCR);
#else
	register_t msr;

	msr = mfmsr();
	/* Freescale E500 core RM section 6.4.1. */
	__asm __volatile("msync; mtmsr %0; isync" ::
	    "r" (msr | PSL_WE));
#endif
}

static int
mpc85xx_idle_wakeup(platform_t plat, int cpu)
{
#ifdef QORIQ_DPAA
	uint32_t reg;

	reg = ccsr_read4(OCP85XX_RCPM_CDOZCR);
	ccsr_write4(OCP85XX_RCPM_CDOZCR, reg & ~(1 << cpu));
	ccsr_read4(OCP85XX_RCPM_CDOZCR);

	return (1);
#endif
	return (0);
}
