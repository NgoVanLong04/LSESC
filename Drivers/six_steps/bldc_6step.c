/*
 * bldc_6step.c - 6-step commutation on TIM1, 6-PWM, STM32F4.
 *
 * Phase A = CH1/CH1N, phase B = CH2/CH2N, phase C = CH3/CH3N.
 * Each phase is in one of three states:
 *   PH_PWM   : CHx = PWM(duty), CHxN = complementary (with dead time)
 *   PH_LOW   : CHx = 0, CHxN = 1  (low-side MOSFET on)
 *   PH_FLOAT : CCxE = CCxNE = 0   (both MOSFETs off, Hi-Z)
 *
 * CubeMX: TIM1 CH1..CH3 "PWM Generation CHx CHxN", dead time set (start with
 * about 500 ns), break disabled, INL pins are TIM1_CHxN (not GPIO).
 */

#include "main.h"
#include "hal_timer.h"
#include "bldc_6step.h"
#include "drv8301.h"
#include "hal_config.h"

volatile int g_comm_offset = 4;		/* tuned: with reverse = 1, 4 = clockwise on this motor */
volatile int g_comm_offset_ccw = 0;	/* tuned: 0 = counter-clockwise on this motor */
volatile int g_comm_dir = 1;		/* +1 = clockwise, -1 = counter-clockwise (uses g_comm_offset_ccw) */
volatile int g_comm_reverse = 1;	/* 1 if hall order is opposite to the phase order */

/* index = 6 - phase (phase 6 -> 0 ... phase 1 -> 5); 0 = A, 1 = B, 2 = C */
static const int8_t k_hi[6] = { 1, 0, 0, 2, 2, 1 };
static const int8_t k_lo[6] = { 2, 2, 1, 1, 0, 0 };

static drv8301_t s_drv;


void bldc_all_float(void) {
	tim_set_duty(0.0f, 0.0f, 0.0f);
	tim_set_mode(PH_FLOAT, PH_FLOAT, PH_FLOAT);
}

int set_phase(int phase, float duty) {
	if (phase < 1 || phase > 6) {
		bldc_all_float();
		return -1;
	}

	int base = g_comm_reverse ? phase : (6 - phase);
	int off = (g_comm_dir >= 0) ? g_comm_offset : g_comm_offset_ccw;
	int step = (base + off) % 6;
	int hi = k_hi[step];
	int lo = k_lo[step];

	phase_state_t mode[3] = { PH_FLOAT, PH_FLOAT, PH_FLOAT };
	float duty3[3] = { 0.0f, 0.0f, 0.0f };

	mode[hi] = PH_PWM;
	duty3[hi] = duty;
	mode[lo] = PH_LOW;

	/* Duty first, then the states, so a phase that becomes PWM is already
	 * loaded with its duty when its outputs are enabled. */
	tim_set_duty(duty3[0], duty3[1], duty3[2]);
	tim_set_mode(mode[0], mode[1], mode[2]);
	return 0;
}

static int drv_start(int *faults) {
	int r;
	uint16_t v;

	hal_spi_dev_t *spi = hal_spi_init();
	if (spi == NULL) {
		return -1;
	}

	if ((r = drv8301_init(&s_drv, spi, DRV_AMP_GAIN)) != 0) return r;
	if ((r = drv8301_set_pwm_mode(&s_drv, DRV8301_PWM_MODE_6)) != 0) return r;
	if ((r = drv8301_set_gate_current(&s_drv, DRV_GATE_CURRENT)) != 0) return r;
	if ((r = drv8301_set_oc_adj(&s_drv, DRV_OC_ADJ)) != 0) return r;
	if ((r = drv8301_set_oc_mode(&s_drv, DRV_OC_MODE)) != 0) return r;

	/* Read back CONTROL1: also proves the SPI mode is right */
	uint16_t expect = (uint16_t)((DRV_OC_ADJ << 6) | (DRV_OC_MODE << 4) |
			(DRV8301_PWM_MODE_6 << 3) | DRV_GATE_CURRENT);
	if ((r = drv8301_read_reg(&s_drv, 2, &v)) != 0) return r;
	if (v != expect) return -2;

	if ((r = drv8301_read_faults(&s_drv, faults)) != 0) return r;
	return (*faults != 0) ? -3 : 0;
}

int bldc_init(void) {
	int fault_read;
	tim_set_duty(0.0f, 0.0f, 0.0f);
	bldc_all_float();
	tim_init();

	drv_start(&fault_read);
	return fault_read;
}
