/*
 * hall_comm.c - hall edge -> commutation, with a delay of a fraction of the
 * previous sector time (see hall_comm.h).
 *
 * TIM2 (32-bit) counts microseconds and never stops. Channel 1 is used as a
 * one-shot compare: on a hall edge CCR1 = now + delay, and the compare
 * interrupt applies the step.
 *
 * The hall EXTI and TIM2 interrupts have the same priority, so they never
 * preempt each other, and both are masked by taskENTER_CRITICAL().
 */

#include "main.h"
#include "hal_sensor.h"
#include "bldc_6step.h"
#include "hall_comm.h"

/* Hall step sign seen when the motor turns the way g_comm_dir = +1 drives it.
 * Check: with a positive duty command the logged speed must be positive.
 * If it is negative, change this to (-1). */
#define HALL_STEP_SIGN_FWD		(+1)

/* A sector longer than this means "too slow to predict": commutate on the edge. */
#define HALL_COMM_MAX_SECTOR_US	20000u

/* Shorter delays are not worth a timer interrupt. */
#define HALL_COMM_MIN_DELAY_US	10u

#define HALL_COMM_FRAC_MAX		0.9f

volatile float g_comm_delay_frac = 0.5f;
volatile float g_comm_delay_frac_ccw = 0.5f;
volatile uint32_t g_comm_sector_us;
volatile uint32_t g_comm_delayed;

static volatile float s_duty;
static volatile int s_sensed = -1;		/* last hall state seen */
static volatile int s_applied = -1;		/* step that is on the outputs now */
static volatile int s_pending = -1;		/* step waiting for the timer */
static volatile int s_last_step;		/* direction of the previous edge */
static volatile int s_edge_valid;		/* s_edge_us holds a real edge time */
static volatile uint32_t s_edge_us;

static void cancel_pending(void) {
	TIM2->DIER &= ~TIM_DIER_CC1IE;
}

static void apply_now(int phase) {
	cancel_pending();
	s_applied = phase;
	set_phase(phase, s_duty);
}

void hall_comm_init(void) {
	__HAL_RCC_TIM2_CLK_ENABLE();
	__HAL_DBGMCU_FREEZE_TIM2();			/* counter stops while the debugger halts the core */

	uint32_t clk = HAL_RCC_GetPCLK1Freq();
	if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
		clk *= 2u;						/* timer clock is 2 x PCLK1 when APB1 is divided */
	}

	TIM2->CR1 = 0;
	TIM2->DIER = 0;
	TIM2->CCMR1 = 0;					/* CH1 frozen compare, no preload on CCR1 */
	TIM2->CCER = 0;
	TIM2->PSC = clk / 1000000u - 1u;	/* 1 tick = 1 us */
	TIM2->ARR = 0xFFFFFFFFu;
	TIM2->EGR = TIM_EGR_UG;				/* load the prescaler */
	TIM2->SR = 0;
	TIM2->CR1 = TIM_CR1_CEN;

	s_edge_valid = 0;
	s_sensed = read_current_phase();
	s_applied = s_sensed;

	HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);	/* same as EXTI9_5 (hall) */
	HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void hall_comm_on_edge(int phase, int step) {
	const uint32_t now = TIM2->CNT;

	if (phase < 1) {					/* invalid hall: float the outputs */
		s_sensed = phase;
		s_edge_valid = 0;
		apply_now(phase);
		return;
	}

	if (phase == s_sensed) {
		return;							/* bounce, state did not change */
	}

	const uint32_t dt = now - s_edge_us;
	const int dir = (g_comm_dir >= 0) ? 1 : -1;

	/* The previous sector time predicts this one only if both edges are single
	 * steps in the same direction, and that direction is the driven one. */
	const int timed = s_edge_valid
			&& (step == 1 || step == -1)
			&& step == s_last_step
			&& step * HALL_STEP_SIGN_FWD == dir
			&& dt <= HALL_COMM_MAX_SECTOR_US;

	s_sensed = phase;
	s_edge_us = now;
	s_edge_valid = 1;
	s_last_step = step;

	uint32_t delay = 0;
	if (timed) {
		float frac = (dir > 0) ? g_comm_delay_frac : g_comm_delay_frac_ccw;
		if (frac < 0.0f) frac = 0.0f;
		if (frac > HALL_COMM_FRAC_MAX) frac = HALL_COMM_FRAC_MAX;
		delay = (uint32_t)((float)dt * frac);
		g_comm_sector_us = dt;
	}

	if (delay < HALL_COMM_MIN_DELAY_US) {
		apply_now(phase);
		return;
	}

	const uint32_t target = now + delay;
	s_pending = phase;
	TIM2->CCR1 = target;
	TIM2->SR = ~TIM_SR_CC1IF;			/* drop any old match */
	TIM2->DIER |= TIM_DIER_CC1IE;
	if ((int32_t)(TIM2->CNT - target) >= 0) {
		TIM2->EGR = TIM_EGR_CC1G;		/* already past the target: fire now */
	}
	g_comm_delayed++;
}

void TIM2_IRQHandler(void) {
	if ((TIM2->DIER & TIM_DIER_CC1IE) && (TIM2->SR & TIM_SR_CC1IF)) {
		TIM2->DIER &= ~TIM_DIER_CC1IE;
		TIM2->SR = ~TIM_SR_CC1IF;
		s_applied = s_pending;
		set_phase(s_applied, s_duty);
	}
}

void hall_comm_refresh(float duty) {
	s_duty = duty;

	if (!s_edge_valid || (TIM2->CNT - s_edge_us) > HALL_COMM_MAX_SECTOR_US) {
		/* Stopped or very slow: no edges to rely on, follow the sensor. */
		cancel_pending();
		s_edge_valid = 0;
		s_sensed = read_current_phase();
		s_applied = s_sensed;
	}

	set_phase(s_applied, duty);
}
