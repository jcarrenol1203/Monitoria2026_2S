/* Core/Src/main.c — the same for every stage; never edited again */
#include "stm32f4xx_hal.h"
#include "system.h"
#include "heartbeat.h"
#include "app.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    heartbeat_init(250U);   /* toggle every 250 ms = 2 Hz blink */
    app_init();             /* provided by the selected stage */
    while (1) {
        app_loop();         /* provided by the selected stage */
    }
}
