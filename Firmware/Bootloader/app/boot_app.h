#ifndef BOOT_APP_H
#define BOOT_APP_H

/** Initialise Bootloader application services after CubeMX initialisation. */
void boot_app_init(void);

/** Run one iteration of the Bootloader application state machine. */
void boot_app_process(void);

#endif /* BOOT_APP_H */
