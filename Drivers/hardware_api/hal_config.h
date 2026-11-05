#ifndef HAL_CONFIG
#define HAL_CONFIG

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define MAX_ARR ARR_TIM

#define INL_A_Pin GPIO_PIN_13
#define INL_A_GPIO_Port GPIOB
#define INL_B_Pin GPIO_PIN_14
#define INL_B_GPIO_Port GPIOB
#define INL_C_Pin GPIO_PIN_15
#define INL_C_GPIO_Port GPIOB

/* ---- Tunable parameters, see the notes on how to choose them ---- */
#define DRV_GATE_CURRENT	DRV8301_GATE_CURRENT_0P25A
#define DRV_OC_MODE			DRV8301_OC_MODE_LATCH_SHUTDOWN
#define DRV_OC_ADJ			10		/* placeholder: pick from the OC_ADJ_SET table */
#define DRV_AMP_GAIN		10		/* internal amp unused, keep a valid value */


#ifdef __cplusplus
}
#endif

#endif
