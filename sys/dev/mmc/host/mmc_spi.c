/*-
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/mmc/host/mmc_spi.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "spibus_if.h"
#include "mmcbr_if.h"

#define	R1_SPI_ERR_NONE		(0)
#define	R1_SPI_ERR_IDLE		(1 << 0)
#define	R1_SPI_ERR_ERASE_RST	(1 << 1)
#define	R1_SPI_ERR_ILLEGAL	(1 << 2)
#define	R1_SPI_ERR_CRC		(1 << 3)
#define	R1_SPI_ERR_ERASE	(1 << 4)
#define	R1_SPI_ERR_ADDR		(1 << 5)
#define	R1_SPI_ERR_PARAM	(1 << 6)

#define dprintf(x, arg...)	printf(x, arg)

#define	READ4(_sc, _reg) \
	bus_read_4((_sc)->res[0], _reg)
#define	WRITE4(_sc, _reg, _val) \
	bus_write_4((_sc)->res[0], _reg, _val)

#define	MMC_SPI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MMC_SPI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	MMC_SPI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "mmc_spi", MTX_DEF)
#define	MMC_SPI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	MMC_SPI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	MMC_SPI_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static uint8_t
crc7(uint8_t crc, const uint8_t *buffer, size_t len)
{
	uint8_t data;

	while (len--) {
		data = *buffer++;
		crc = crc7_be_syndrome[crc ^ data];
	}

	return crc;
}

static int
mmc_spi_probe(device_t dev)
{

	device_set_desc(dev, "MMC SPI");
	return (BUS_PROBE_DEFAULT);
}

static int
mmc_spi_attach(device_t dev)
{
	struct mmc_spi_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	MMC_SPI_LOCK_INIT(sc);

	sc->host.f_min = 400000;
	sc->host.f_max = 20000000;
	sc->host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
	sc->host.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_SPI;

	device_add_child(dev, "mmc", -1);
	return (bus_generic_attach(dev));
}

static int
mmc_spi_update_ios(device_t brdev, device_t reqdev)
{
	struct mmc_spi_softc *sc;
	struct mmc_ios *ios;

	sc = device_get_softc(brdev);
	ios = &sc->host.ios;

	return (0);
}

static uint8_t
xchg_spi(struct mmc_spi_softc *sc, uint8_t byte)
{
	struct spi_command spi_cmd;
	uint8_t msg_dinp;

	msg_dinp = 0;

	memset(&spi_cmd, 0, sizeof(spi_cmd));
	spi_cmd.tx_cmd = &byte;
	spi_cmd.rx_cmd = &msg_dinp;
	spi_cmd.tx_cmd_sz = 1;
	spi_cmd.rx_cmd_sz = 1;

	SPIBUS_TRANSFER(device_get_parent(sc->dev), sc->dev, &spi_cmd);

	return (msg_dinp);
}

static uint8_t
xchg_spi_multi(struct mmc_spi_softc *sc, uint8_t *out_bytes,
    uint8_t *in_bytes, uint32_t nbytes)
{
	struct spi_command spi_cmd;

	memset(&spi_cmd, 0, sizeof(spi_cmd));
	spi_cmd.tx_cmd = out_bytes;
	spi_cmd.rx_cmd = in_bytes;
	spi_cmd.tx_cmd_sz = nbytes;
	spi_cmd.rx_cmd_sz = nbytes;

	SPIBUS_TRANSFER(device_get_parent(sc->dev), sc->dev, &spi_cmd);

	return (0);
}

static int
wait_ready(struct mmc_spi_softc *sc, int timeout)
{
	int i;

	for (i = 0; i < (timeout * 5000); i++) {
		if (xchg_spi(sc, 0xff) == 0xff)
			return (1);	/* Ready */
	}

	/* Timeout */
	return (0);
}

static int
mmc_cmd_done(struct mmc_spi_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;
	uint32_t timeout;
	int block_count;
	uint8_t *ptr;
	int reg;
	int j;
	int i;

	data = cmd->data;

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {

			/* Wait for data */
			timeout = 2000;
			do {
				reg = xchg_spi(sc, 0xff);
				timeout--;
			} while ((reg == 0xff) && timeout);
			if (reg != 0xFE) {
				printf("FAILED to get RSP136\n");
			}

			for (j = 0; j < 4; j++) {
				reg = 0;
				for (i = 3; i >= 0; i--) {
					reg |= (xchg_spi(sc, 0xff) << (i * 8));
				}
				//dprintf("long resp(%d): 0x%08x\n", j, reg);
				cmd->resp[j] = reg;
			}
		} else if (cmd->opcode != 17 && cmd->opcode != 18) {
			reg = 0;
			for (i = 3; i >= 0; i--) {
				reg |= (xchg_spi(sc, 0xff) << (i * 8));
			}
			//dprintf("short resp0: 0x%08x\n", reg);
			cmd->resp[0] = reg;
		}
	}

	if (cmd->opcode == 17 || cmd->opcode == 18) {
		block_count = (data->len / 512);
		ptr = data->data;

		for (j = 0; j < block_count; j++) {
			/* Wait for data */
			timeout = 2000;
			do {
				reg = xchg_spi(sc, 0xff);
				timeout--;
			} while ((reg == 0xff) && timeout);
			if (reg != 0xFE) {
				printf("FAILED to wait DATA\n");
			}
			for (i = 0; i < 512; i++) {
				reg = xchg_spi(sc, 0xff);
				*ptr++ = reg;
				//printf("%x ", reg);
			}
			xchg_spi(sc, 0xFF);	/* Skip CRC */
			xchg_spi(sc, 0xFF);
		}
		//printf("\n");
	}

	xchg_spi(sc, 0xff);

	return (0);
}

static int
mmc_spi_req(struct mmc_spi_softc *sc, struct mmc_command *cmd)
{
	uint8_t req_in[7];
	uint8_t req[7];
	int ret;
	int i;

	req[0] = 0xff;
	req[1] = (0x40 | cmd->opcode);
	req[2] = (cmd->arg >> 24);
	req[3] = (cmd->arg >> 16);
	req[4] = (cmd->arg >> 8);
	req[5] = (cmd->arg >> 0);
	req[6] = crc7(0, &req[1], 5) | 0x01;
	xchg_spi_multi(sc, req, req_in, 7);

	/* Wait response */
	for (i = 0; i < 1000; i++) {
		ret = xchg_spi(sc, 0xff);
		if ((ret & 0x80) == 0)
			break;
	}

	cmd->error = MMC_ERR_TIMEOUT;
	if (cmd->opcode == ACMD_SD_SEND_OP_COND || \
	    cmd->opcode == MMC_SEND_CSD || \
	    cmd->opcode == MMC_SEND_CID || \
	    cmd->opcode == MMC_SPI_READ_OCR) {
		if (ret == R1_SPI_ERR_NONE)
			cmd->error = MMC_ERR_NONE;
	} else if (ret == R1_SPI_ERR_IDLE || ret == R1_SPI_ERR_NONE) {
		cmd->error = MMC_ERR_NONE;
	}

	if (cmd->error)
		return (1);

	mmc_cmd_done(sc, cmd);
	return (0);
}


static int
mmc_spi_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct mmc_command *cmd;
	struct mmc_spi_softc *sc;

	sc = device_get_softc(brdev);

	MMC_SPI_LOCK(sc);

	cmd = req->cmd;

	SPIBUS_CHIP_SELECT(device_get_parent(sc->dev), sc->dev);
	if (!wait_ready(sc, 500)) {
		cmd->error = MMC_ERR_TIMEOUT;
		req->done(req);
		MMC_SPI_UNLOCK(sc);
		return (1);
	}

	mmc_spi_req(sc, req->cmd);
	if (req->stop) {
		mmc_spi_req(sc, req->stop);
	}
	SPIBUS_CHIP_DESELECT(device_get_parent(sc->dev), sc->dev);

	req->done(req);

	MMC_SPI_UNLOCK(sc);

	return (0);
}

static int
mmc_spi_get_ro(device_t brdev, device_t reqdev)
{

	return (0);
}

static int
mmc_spi_acquire_host(device_t brdev, device_t reqdev)
{
	struct mmc_spi_softc *sc;

	sc = device_get_softc(brdev);

	MMC_SPI_LOCK(sc);
	while (sc->bus_busy)
		msleep(sc, &sc->sc_mtx, PZERO, "mmc_spiah", hz / 5);
	sc->bus_busy++;
	MMC_SPI_UNLOCK(sc);
	return (0);
}

static int
mmc_spi_release_host(device_t brdev, device_t reqdev)
{
	struct mmc_spi_softc *sc;

	sc = device_get_softc(brdev);

	MMC_SPI_LOCK(sc);
	sc->bus_busy--;
	wakeup(sc);
	MMC_SPI_UNLOCK(sc);
	return (0);
}

static int
mmc_spi_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct mmc_spi_softc *sc;

	sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = sc->host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = sc->host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = sc->host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = sc->host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = sc->host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = sc->host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = sc->host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = sc->host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = sc->host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = sc->host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = sc->host.ios.vdd;
		break;
	case MMCBR_IVAR_CAPS:
		sc->host.caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA;
		*(int *)result = sc->host.caps;
		break;
	case MMCBR_IVAR_MAX_DATA:
		*(int *)result = 65535;
	}
	return (0);
}

static int
mmc_spi_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct mmc_spi_softc *sc;

	sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		sc->host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		sc->host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		sc->host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		sc->host.ios.clock = value;
		break;
	case MMCBR_IVAR_MODE:
		sc->host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		sc->host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		sc->host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		sc->host.ios.vdd = value;
		break;
	/* These are read-only */
	case MMCBR_IVAR_CAPS:
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	case MMCBR_IVAR_MAX_DATA:
		return (EINVAL);
	}
	return (0);
}

static device_method_t mmc_spi_methods[] = {
	DEVMETHOD(device_probe,		mmc_spi_probe),
	DEVMETHOD(device_attach,	mmc_spi_attach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	mmc_spi_read_ivar),
	DEVMETHOD(bus_write_ivar,	mmc_spi_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios,	mmc_spi_update_ios),
	DEVMETHOD(mmcbr_request,	mmc_spi_request),
	DEVMETHOD(mmcbr_get_ro,		mmc_spi_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	mmc_spi_acquire_host),
	DEVMETHOD(mmcbr_release_host,	mmc_spi_release_host),

	DEVMETHOD_END
};

driver_t mmc_spi_driver = {
	"mmc_spi",
	mmc_spi_methods,
	sizeof(struct mmc_spi_softc),
};

static devclass_t mmc_spi_devclass;

DRIVER_MODULE(mmc_spi, spibus, mmc_spi_driver, mmc_spi_devclass, 0, 0);
MODULE_DEPEND(mmc_spi, spibus, 1, 1, 1);
MODULE_DEPEND(mmc_spi, mmc, 1, 1, 1);
MODULE_VERSION(mmc_spi, 1);
DRIVER_MODULE(mmc, mmc_spi, mmc_driver, mmc_devclass, NULL, NULL);
