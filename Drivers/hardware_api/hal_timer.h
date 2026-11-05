#ifndef HAL_TIMER
#define HAL_TIMER

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#define OCM_PWM1			6u		/* OCxM = 110 */
#define OCM_FORCE_INACTIVE	4u		/* OCxM = 100 -> OCxREF = 0 */
#define DUTY_MAX			0.95f

typedef enum { PH_FLOAT = 0, PH_LOW, PH_PWM } phase_state_t;

void tim_set_duty(float a, float b, float c);
void tim_set_mode(phase_state_t a, phase_state_t b, phase_state_t c);

void phase_apply(int ph, phase_state_t s, uint32_t ccr);
void tim_init(void);

#ifdef __cplusplus
}
#endif

#endif
