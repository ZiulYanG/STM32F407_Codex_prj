#ifndef APP_MAIN_H
#define APP_MAIN_H

/** Queue the Application startup diagnostics before the scheduler starts. */
void app_main_init(void);

/** Run the main Application heartbeat task. This function never returns. */
void app_main_task(void *argument);

#endif /* APP_MAIN_H */
