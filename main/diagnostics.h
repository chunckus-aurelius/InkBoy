#pragma once
#ifdef INKBOY_DIAGNOSTICS
int diagnostics_gpio(int pin);
int diagnostics_controller(void);
void diagnostics_start(void);
#endif
