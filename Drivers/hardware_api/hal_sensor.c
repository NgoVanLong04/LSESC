#include "hal_sensor.h"
#include "main.h"

int read_current_phase(void){
	GPIO_PinState hall_a, hall_b, hall_c;
	hall_a = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6); 	// hall 1
	hall_b = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7);	// hall 2
	hall_c = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8);	// hall 3

	int temp = (hall_c << 2) | (hall_b << 1) | hall_a;
	switch(temp){
		case 0b100:{
			return 6;
		}
		case 0b110:{
			return 5;
		}
		case 0b010:{
			return 4;
		}
		case 0b011:{
			return 3;
		}
		case 0b001:{
			return 2;
		}
		case 0b101:{
			return 1;
		}
	}
	return -1;
}

