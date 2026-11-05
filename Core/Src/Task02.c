#include "main.h"
#include "cmsis_os.h"
#include "stm32_log.h"

void StartTask02(void *argument)
{

  for(;;)
  {
    osDelay(1);
    log_handler();
  }
}
