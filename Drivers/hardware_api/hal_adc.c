#include "hal_adc.h"
#include "hal_config.h"
#include "adc.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

HAL_StatusTypeDef adc_init(uint32_t* raw_vbus){
	HAL_StatusTypeDef res = HAL_OK;
	if ((res = HAL_ADCEx_InjectedStart(&hadc3)) != HAL_OK) return res;
	if ((res = HAL_ADCEx_InjectedStart(&hadc2)) != HAL_OK) return res;
	if ((res = HAL_ADCEx_InjectedStart_IT(&hadc1)) != HAL_OK) return res;
//	if ((res = HAL_ADC_Start_DMA(&hadc1, raw_vbus, 1)) != HAL_OK) return res;
	return res;
}
