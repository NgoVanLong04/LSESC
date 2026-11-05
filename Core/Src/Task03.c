#include "main.h"
#include "cmsis_os.h"

void StartTask03(void *argument)
{

  for(;;)
  {
    osDelay(1);
    if(HAL_GPIO_ReadPin(NFAULT_GPIO_Port, NFAULT_Pin) == GPIO_PIN_RESET){
//    	  HAL_GPIO_WritePin(EN_GATE_GPIO_Port, EN_GATE_Pin, GPIO_PIN_RESET);
    }
  }
}
