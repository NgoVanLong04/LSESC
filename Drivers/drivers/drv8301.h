/*
 * drv8301.h - Hardware independent DRV8301 gate driver (SPI configuration).
 *
 * Based on the VESC firmware driver (c) Benjamin Vedder, GPLv3.
 * Platform access goes through hal_spi.h only.
 */

#ifndef DRV8301_H_
#define DRV8301_H_

#include <stdint.h>
#include <stddef.h>
#include "hal_spi.h"

/* Over-current protection mode (CONTROL1 bits 5:4) */
typedef enum {
	DRV8301_OC_MODE_CURRENT_LIMIT = 0,
	DRV8301_OC_MODE_LATCH_SHUTDOWN = 1,
	DRV8301_OC_MODE_REPORT_ONLY = 2,
	DRV8301_OC_MODE_DISABLED = 3
} drv8301_oc_mode_t;

/* PWM input mode (CONTROL1 bit 3) */
typedef enum {
	DRV8301_PWM_MODE_6 = 0,  /* INH_x / INL_x independent */
	DRV8301_PWM_MODE_3 = 1   /* INH_x = PWM, INL_x = enable of phase x */
} drv8301_pwm_mode_t;

/* Peak gate drive current (CONTROL1 bits 1:0) */
typedef enum {
	DRV8301_GATE_CURRENT_1P7A = 0,
	DRV8301_GATE_CURRENT_0P7A = 1,
	DRV8301_GATE_CURRENT_0P25A = 2
} drv8301_gate_current_t;

typedef struct {
	hal_spi_dev_t *spi;
	int amp_gain;            /* 10, 20, 40 or 80 */
} drv8301_t;

/* Fault bits returned by drv8301_read_faults() */
#define DRV8301_FAULT_FETLC_OC		(1 << 0)
#define DRV8301_FAULT_FETHC_OC		(1 << 1)
#define DRV8301_FAULT_FETLB_OC		(1 << 2)
#define DRV8301_FAULT_FETHB_OC		(1 << 3)
#define DRV8301_FAULT_FETLA_OC		(1 << 4)
#define DRV8301_FAULT_FETHA_OC		(1 << 5)
#define DRV8301_FAULT_OTW			(1 << 6)
#define DRV8301_FAULT_OTSD			(1 << 7)
#define DRV8301_FAULT_PVDD_UV		(1 << 8)
#define DRV8301_FAULT_GVDD_UV		(1 << 9)
#define DRV8301_FAULT_FAULT			(1 << 10)
#define DRV8301_FAULT_GVDD_OV		(1 << 11)

/* All functions return 0 on success and a negative value on error,
 * unless stated otherwise. */

/**
 * Initialize the driver. EN_GATE must already be high and the chip must have
 * had time to power up (~10 ms) before calling this.
 */
int drv8301_init(drv8301_t *dev, hal_spi_dev_t *spi, int amp_gain);

/** Read a register (11 data bits). */
int drv8301_read_reg(drv8301_t *dev, uint8_t reg, uint16_t *val);

/** Write a register (11 data bits). */
int drv8301_write_reg(drv8301_t *dev, uint8_t reg, uint16_t data);

/**
 * Set OC threshold. val in [0, 31], lower = lower current limit
 * (see datasheet for the Vds table).
 */
int drv8301_set_oc_adj(drv8301_t *dev, int val);

int drv8301_set_oc_mode(drv8301_t *dev, drv8301_oc_mode_t mode);

int drv8301_set_pwm_mode(drv8301_t *dev, drv8301_pwm_mode_t mode);

/** Gate drive peak current: sets the MOSFET switching speed. */
int drv8301_set_gate_current(drv8301_t *dev, drv8301_gate_current_t current);

/** Shunt amplifier gain: 10, 20, 40 or 80. */
int drv8301_set_current_amp_gain(drv8301_t *dev, int gain);

/** Read faults into *faults (DRV8301_FAULT_* bits). */
int drv8301_read_faults(drv8301_t *dev, int *faults);

/** Clear latched faults and restore the amplifier gain. */
int drv8301_reset_faults(drv8301_t *dev);

/**
 * Convert fault bits to text in the caller's buffer (thread safe).
 * Returns buf.
 */
const char *drv8301_faults_to_string(int faults, char *buf, size_t len);

#endif /* DRV8301_H_ */
