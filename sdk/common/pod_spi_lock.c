#include "pod_spi_lock.h"
#include "pico/mutex.h"

auto_init_recursive_mutex(pod_spi_mutex);

void pod_spi_lock(void)   { recursive_mutex_enter_blocking(&pod_spi_mutex); }
void pod_spi_unlock(void) { recursive_mutex_exit(&pod_spi_mutex); }
