/* Core/common/app.h — the two functions every stage provides */
#ifndef APP_H
#define APP_H

void app_init(void);   /* called once, after the clock and the heartbeat are running */
void app_loop(void);   /* called forever from the while (1) loop in main() */

#endif
