/*
 * drv8301.c - Hardware independent DRV8301 gate driver (SPI configuration).
 *
 * Based on the VESC firmware driver (c) Benjamin Vedder, GPLv3.
 */

#include "drv8301.h"
#include <stdio.h>
#include <string.h>

/* Registers */
#define REG_STATUS1		0
#define REG_STATUS2		1
#define REG_CTRL1		2
#define REG_CTRL2		3

/* CONTROL1 (0x02) */
#define CTRL1_GATE_CURRENT_MASK	0x3u
#define CTRL1_GATE_RESET		(1u << 2)
#define CTRL1_PWM_MODE			(1u << 3)
#define CTRL1_OC_MODE_SHIFT		4
#define CTRL1_OC_MODE_MASK		(0x3u << CTRL1_OC_MODE_SHIFT)
#define CTRL1_OC_ADJ_SHIFT		6
#define CTRL1_OC_ADJ_MASK		(0x1Fu << CTRL1_OC_ADJ_SHIFT)

/* CONTROL2 (0x03) */
#define CTRL2_GAIN_SHIFT		2
#define CTRL2_GAIN_MASK			(0x3u << CTRL2_GAIN_SHIFT)

#define DATA_MASK				0x07FF

/* OC disabled, OC_ADJ = 16 */
#define CTRL1_INIT_VALUE		0x0430

/* ---- Frame helpers (caller must hold the SPI lock) ---- */

static int write_locked(drv8301_t *dev, uint8_t reg, uint16_t data) {
	uint16_t frame = (uint16_t)(((reg & 0x0F) << 11) | (data & DATA_MASK));
	return hal_spi_transfer16(dev->spi, frame, NULL);
}

static int read_locked(drv8301_t *dev, uint8_t reg, uint16_t *val) {
	int res;

	/* Same behaviour as the VESC driver: status register 1 is returned
	 * by a plain frame, the others need a read command first. */
	if (reg != REG_STATUS1) {
		uint16_t cmd = (uint16_t)((1u << 15) | ((reg & 0x0F) << 11));
		res = hal_spi_transfer16(dev->spi, cmd, NULL);
		if (res != 0) {
			return res;
		}
	}

	uint16_t rx = 0;
	res = hal_spi_transfer16(dev->spi, 0xFFFF, &rx);
	if (res != 0) {
		return res;
	}

	*val = rx & DATA_MASK;
	return 0;
}

static int modify_locked(drv8301_t *dev, uint8_t reg, uint16_t mask, uint16_t bits) {
	uint16_t v;
	int res = read_locked(dev, reg, &v);
	if (res != 0) {
		return res;
	}
	v = (uint16_t)((v & ~mask) | (bits & mask));
	return write_locked(dev, reg, v);
}

/* Read-modify-write under one lock */
static int modify(drv8301_t *dev, uint8_t reg, uint16_t mask, uint16_t bits) {
	int res = hal_spi_lock(dev->spi);
	if (res != 0) {
		return res;
	}
	res = modify_locked(dev, reg, mask, bits);
	hal_spi_unlock(dev->spi);
	return res;
}

/* ---- Public API ---- */

int drv8301_init(drv8301_t *dev, hal_spi_dev_t *spi, int amp_gain) {
	dev->spi = spi;
	dev->amp_gain = amp_gain;

	int res = hal_spi_lock(dev->spi);
	if (res != 0) {
		return res;
	}

	/* Disable OC. Written twice like the original driver, the first frame
	 * after power-up may be ignored. */
	res = write_locked(dev, REG_CTRL1, CTRL1_INIT_VALUE);
	if (res == 0) {
		res = write_locked(dev, REG_CTRL1, CTRL1_INIT_VALUE);
	}

	hal_spi_unlock(dev->spi);

	if (res != 0) {
		return res;
	}

	return drv8301_set_current_amp_gain(dev, amp_gain);
}

int drv8301_read_reg(drv8301_t *dev, uint8_t reg, uint16_t *val) {
	int res = hal_spi_lock(dev->spi);
	if (res != 0) {
		return res;
	}
	res = read_locked(dev, reg, val);
	hal_spi_unlock(dev->spi);
	return res;
}

int drv8301_write_reg(drv8301_t *dev, uint8_t reg, uint16_t data) {
	int res = hal_spi_lock(dev->spi);
	if (res != 0) {
		return res;
	}
	res = write_locked(dev, reg, data);
	hal_spi_unlock(dev->spi);
	return res;
}

int drv8301_set_oc_adj(drv8301_t *dev, int val) {
	if (val < 0 || val > 31) {
		return -1;
	}
	return modify(dev, REG_CTRL1, CTRL1_OC_ADJ_MASK,
			(uint16_t)(val << CTRL1_OC_ADJ_SHIFT));
}

int drv8301_set_oc_mode(drv8301_t *dev, drv8301_oc_mode_t mode) {
	return modify(dev, REG_CTRL1, CTRL1_OC_MODE_MASK,
			(uint16_t)((mode & 0x03) << CTRL1_OC_MODE_SHIFT));
}

int drv8301_set_pwm_mode(drv8301_t *dev, drv8301_pwm_mode_t mode) {
	return modify(dev, REG_CTRL1, CTRL1_PWM_MODE,
			mode == DRV8301_PWM_MODE_3 ? CTRL1_PWM_MODE : 0);
}

int drv8301_set_gate_current(drv8301_t *dev, drv8301_gate_current_t current) {
	return modify(dev, REG_CTRL1, CTRL1_GATE_CURRENT_MASK, (uint16_t)(current & 0x03));
}

int drv8301_set_current_amp_gain(drv8301_t *dev, int gain) {
	uint16_t code;

	switch (gain) {
	case 10: code = 0; break;
	case 20: code = 1; break;
	case 40: code = 2; break;
	case 80: code = 3; break;
	default: return -1; /* not supported */
	}

	int res = modify(dev, REG_CTRL2, CTRL2_GAIN_MASK,
			(uint16_t)(code << CTRL2_GAIN_SHIFT));
	if (res == 0) {
		dev->amp_gain = gain;
	}
	return res;
}

int drv8301_read_faults(drv8301_t *dev, int *faults) {
	uint16_t r0, r1;

	int res = hal_spi_lock(dev->spi);
	if (res != 0) {
		return res;
	}

	res = read_locked(dev, REG_STATUS1, &r0);
	if (res == 0) {
		res = read_locked(dev, REG_STATUS2, &r1);
	}

	hal_spi_unlock(dev->spi);

	if (res != 0) {
		return res;
	}

	*faults = (r0 & 0x3FF) | ((r1 & 0x80) << 4);
	return 0;
}

int drv8301_reset_faults(drv8301_t *dev) {
	int res = modify(dev, REG_CTRL1, CTRL1_GATE_RESET, CTRL1_GATE_RESET);
	if (res != 0) {
		return res;
	}
	return drv8301_set_current_amp_gain(dev, dev->amp_gain);
}

const char *drv8301_faults_to_string(int faults, char *buf, size_t len) {
	static const struct {
		int mask;
		const char *name;
	} table[] = {
		{ DRV8301_FAULT_FETLC_OC, "FETLC_OC" },
		{ DRV8301_FAULT_FETHC_OC, "FETHC_OC" },
		{ DRV8301_FAULT_FETLB_OC, "FETLB_OC" },
		{ DRV8301_FAULT_FETHB_OC, "FETHB_OC" },
		{ DRV8301_FAULT_FETLA_OC, "FETLA_OC" },
		{ DRV8301_FAULT_FETHA_OC, "FETHA_OC" },
		{ DRV8301_FAULT_OTW,      "OTW" },
		{ DRV8301_FAULT_OTSD,     "OTSD" },
		{ DRV8301_FAULT_PVDD_UV,  "PVDD_UV" },
		{ DRV8301_FAULT_GVDD_UV,  "GVDD_UV" },
		{ DRV8301_FAULT_FAULT,    "FAULT" },
		{ DRV8301_FAULT_GVDD_OV,  "GVDD_OV" },
	};

	if (buf == NULL || len == 0) {
		return "";
	}

	if (faults == 0) {
		snprintf(buf, len, "No DRV8301 faults");
		return buf;
	}

	snprintf(buf, len, "|");

	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (faults & table[i].mask) {
			size_t n = strlen(buf);
			snprintf(buf + n, len - n, " %s |", table[i].name);
		}
	}

	return buf;
}
