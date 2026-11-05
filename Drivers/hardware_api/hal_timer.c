#include "hal_timer.h"
#include "hal_config.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

static uint32_t duty_to_ccr(float d) {
	if (d < 0.0f) d = 0.0f;
	if (d > DUTY_MAX) d = DUTY_MAX;
	return (uint32_t)(d * (float)MAX_ARR);
}

void tim_set_duty(float a, float b, float c) {
	__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, duty_to_ccr(a));
	__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_2, duty_to_ccr(b));
	__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_3, duty_to_ccr(c));
}

void tim_set_mode(phase_state_t a, phase_state_t b, phase_state_t c) {
	const phase_state_t st[3] = { a, b, c };

	/* Compute the new register values for all three phases first, then write
	 * each register once, so the three phases change together. */
	uint32_t ccmr1 = TIM1->CCMR1;
	uint32_t ccmr2 = TIM1->CCMR2;
	uint32_t ccer = TIM1->CCER;

	for (int ph = 0; ph < 3; ph++) {
		const uint32_t ocm = (st[ph] == PH_PWM) ? OCM_PWM1 : OCM_FORCE_INACTIVE;
		const uint32_t en = 0x5u << (4u * (uint32_t)ph);	/* CCxE | CCxNE */

		if (ph == 0) {
			ccmr1 = (ccmr1 & ~(7u << 4)) | (ocm << 4);		/* OC1M */
		} else if (ph == 1) {
			ccmr1 = (ccmr1 & ~(7u << 12)) | (ocm << 12);	/* OC2M */
		} else {
			ccmr2 = (ccmr2 & ~(7u << 4)) | (ocm << 4);		/* OC3M */
		}

		if (st[ph] == PH_FLOAT) {
			ccer &= ~en;
		} else {
			ccer |= en;
		}
	}

	/* OCxM first, outputs enable/disable last */
	TIM1->CCMR1 = ccmr1;
	TIM1->CCMR2 = ccmr2;
	TIM1->CCER = ccer;
}

void phase_apply(int ph, phase_state_t s, uint32_t ccr) {
	volatile uint32_t *ccmr = (ph == 2) ? &TIM1->CCMR2 : &TIM1->CCMR1;
	const uint32_t sh = (ph == 1) ? 12u : 4u;       /* OCxM field position */
	const uint32_t en = 0x5u << (4u * (uint32_t)ph); /* CCxE | CCxNE */

	uint32_t ocm = (s == PH_PWM) ? OCM_PWM1 : OCM_FORCE_INACTIVE;
	*ccmr = (*ccmr & ~(7u << sh)) | (ocm << sh);

	(&TIM1->CCR1)[ph] = ccr;

	if (s == PH_FLOAT) {
		TIM1->CCER &= ~en;
	} else {
		TIM1->CCER |= en;
	}
}

void tim_init(void){
	TIM1->CCER &= ~0x555u;			/* all six outputs disabled */
	for (int ph = 0; ph < 3; ph++) {
		(&TIM1->CCR1)[ph] = 0;
	}
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
	__HAL_TIM_MOE_ENABLE(&htim1);
}


