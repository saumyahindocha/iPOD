// pod_spi_lock - one lock for the shared SPI0 bus (display, touch and SD card).
//
// The display and touch run on core 1; the SD card is read by core 0 in SD mode.
// Whoever holds the lock owns the bus: they set their own baud rate after
// taking it and must leave the bus at POD_TFT_BAUD when they release it.
// The lock is recursive, so a driver can call helpers that also take it.
#pragma once

void pod_spi_lock(void);
void pod_spi_unlock(void);
