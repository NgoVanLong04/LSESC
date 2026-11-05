#include "main.h"
#include "cmsis_os.h"
#include "hal_adc.h"
#include "math.h"
#include "bldc_6step.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

uint16_t debug_adc_raw[6] = {0};
float debug_adc[6] = {0};
float current_sum = 0.0f;
uint32_t raw_vbus = 0;
float vbus	= 0.0f;
extern float duty_a;

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
    	HAL_GPIO_WritePin(TEST1_GPIO_Port, TEST1_Pin, GPIO_PIN_SET);
        uint32_t current_a = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
        uint32_t voltage_a = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2);
        uint32_t current_b = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1);
		uint32_t voltage_b = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_2);
		uint32_t current_c = HAL_ADCEx_InjectedGetValue(&hadc3, ADC_INJECTED_RANK_1);
		uint32_t voltage_c = HAL_ADCEx_InjectedGetValue(&hadc3, ADC_INJECTED_RANK_2);
		debug_adc_raw[0] = current_a;	debug_adc_raw[1] = current_b;	debug_adc_raw[2] = current_c;
		debug_adc_raw[3] = voltage_a;	debug_adc_raw[4] = voltage_b;	debug_adc_raw[5] = voltage_c;

		debug_adc[0] = adc_to_amps(current_a);	debug_adc[1] = adc_to_amps(current_b);	debug_adc[2] = adc_to_amps(current_c);
		debug_adc[3] = adc_to_vfet(voltage_a);	debug_adc[4] = adc_to_vfet(voltage_b);	debug_adc[5] = adc_to_vfet(voltage_c);
		current_sum = debug_adc[0] + debug_adc[1] + debug_adc[2];

		HAL_GPIO_WritePin(TEST1_GPIO_Port, TEST1_Pin, GPIO_PIN_RESET);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
	if (hadc->Instance == ADC1) {
		vbus = adc_to_vbus(raw_vbus);
	}
}

extern int read_current_phase(void);
extern float duty_a;				/* adjust to match your declaration */
extern volatile int last_phase;		/* init to 0 */
extern volatile int count;			/* written ONLY by this ISR */

#define COUNT_SIGN		(+1)

volatile int hall_err;				/* jumps of more than 1 step, for debugging */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin != GPIO_PIN_6 && GPIO_Pin != GPIO_PIN_7 && GPIO_Pin != GPIO_PIN_8) {
		return;
	}

//	if (!g_hall_commutate) {
//		return;						/* calibration running: do not touch the outputs */
//	}

	int cur = read_current_phase();

	/* Invalid hall (-1) floats all phases inside set_phase() */
	set_phase(cur, duty_a);

	if (cur < 1) {
		hall_err++;
		return;						/* keep last_phase, do not count */
	}

	int last = last_phase;
	last_phase = cur;

	if (last < 1) {
		return;						/* first valid sample, nothing to compare */
	}

	/* Signed distance on the 6-state ring: 1 = one step forward, 5 = one back */
	int step;
	switch ((cur - last + 6) % 6) {
	case 0:  step =  0; break;		/* same state, edge bounce */
	case 1:  step = +1; break;
	case 5:  step = -1; break;
	case 2:  step = +2; hall_err++; break;	/* one edge missed */
	case 4:  step = -2; hall_err++; break;	/* one edge missed */
	default: step =  0; hall_err++; break;	/* 3: direction unknown */
	}

	count += COUNT_SIGN * step;
}
