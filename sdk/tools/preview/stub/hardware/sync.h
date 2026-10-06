#pragma once
static inline unsigned save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(unsigned s) { (void)s; }
#define __dmb() do {} while (0)
