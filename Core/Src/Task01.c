#include "main.h"
#include "cmsis_os.h"
#include "usb_device.h"
#include <stdbool.h>


#include "hal_sensor.h"
#include "bldc_6step.h"
#include "stm32_log.h"
#include "usbd_cdc_if.h"
#include "hal_adc.h"
#include "hal_timer.h"

log_port_ret_t log_port_write(const char *data, uint32_t len)
{
    if (CDC_Transmit_FS((uint8_t *)data, (uint16_t)len) != USBD_OK) {
        return LOG_PORT_BUSY;
    }
    return LOG_PORT_ASYNC;
}

extern uint32_t raw_vbus;
extern bool USB_FloatParser_TakeCommand(float *value);

float duty_a;
volatile int count;
volatile int last_phase;
float cpm;
float hall_speed_erpm(void) {
	static int prev_count = 0;
	static uint32_t prev_ms = 0;
	static float speed = 0.0f;

	uint32_t now_ms = HAL_GetTick();
	uint32_t dt_ms = now_ms - prev_ms;
	if (dt_ms < 20) {
		return speed;
	}

	int now_count = count;			/* 32-bit aligned read is atomic */
	int diff = now_count - prev_count;
	prev_count = now_count;
	prev_ms = now_ms;

	/* 6 counts = 1 electrical rev:  erpm = diff / 6 * 60 / dt_s = 10 * diff / dt_s */
	speed = 10.0f * 1000.0f * (float)diff / (float)dt_ms;
	return speed;
}

#define CAL_STEPS	20

int32_t hall_seq[CAL_STEPS];	/* hall reading after step k, -1 = flicker */
int32_t hall_diff[CAL_STEPS];	/* (hall - i) mod 6, -1 = flicker */

/* Step k applies i = 6 - (k % 6): 6,5,4,3,2,1, 6,5,4,3,2,1, 6,5,4 */
void phase_calib(void) {
	const float duty = 0.12f;		/* holding current about 1 A or less */

	HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);	/* hall ISR must not call set_phase() meanwhile */

	for (int k = 0; k < CAL_STEPS; k++) {
		int i = 6 - (k % 6);

		set_phase(i, duty);
		HAL_Delay(k == 0 ? 500 : 200);	/* first step also pulls the rotor in */

		int h = read_current_phase();
		int stable = 1;
		for (int n = 0; n < 5; n++) {
			HAL_Delay(10);
			if (read_current_phase() != h) {
				stable = 0;
			}
		}

		hall_seq[k] = stable ? h : -1;
		hall_diff[k] = stable ? (((h - i) % 6) + 6) % 6 : -1;
	}

	bldc_all_float();

	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
	HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

GPIO_PinState level[6];
int test = 0;
float test_pwm = 0.0f;
void StartTask01(void *argument)
{
  HAL_GPIO_WritePin(EN_GATE_GPIO_Port, EN_GATE_Pin, GPIO_PIN_SET);
  osDelay(10);
  MX_USB_DEVICE_Init();
  taskENTER_CRITICAL();
  adc_init(&raw_vbus);
  bldc_init();
  taskEXIT_CRITICAL();
//   phase_calib();
  last_phase = read_current_phase();
  set_phase(last_phase, duty_a);


  for(;;)
  {
    osDelay(10);
    float cmd_duty = 0.0f;
    if(USB_FloatParser_TakeCommand(&cmd_duty)){
    	taskENTER_CRITICAL();
    	duty_a = cmd_duty >= 0.0f ? cmd_duty : -cmd_duty;
    	g_comm_dir = cmd_duty >= 0 ? 1 : -1;
    	set_phase(last_phase, duty_a);
		taskEXIT_CRITICAL();
    }
    taskENTER_CRITICAL();
    set_phase(read_current_phase(), duty_a);
    taskEXIT_CRITICAL();
    cpm = hall_speed_erpm();
    LOGI("SPEED", "%0.2f", cpm);
  }
}
