/*
 * hal_spi.h - SPI abstraction used by the DRV8301 driver.
 *
 * Implement these three functions for your platform (hardware SPI, bit-bang,
 * ChibiOS, FreeRTOS, Zephyr, ...). The driver never touches GPIO/SPI registers
 * or any OS API directly.
 *
 * DRV8301 SPI requirements:
 *   - 16-bit frames, MSB first
 *   - CPOL = 0, CPHA = 1 (SPI mode 1): data shifted out on rising SCK edge,
 *     captured on falling SCK edge
 *   - nSCS active low, must be released between frames
 *   - SCK <= 10 MHz
 */

#ifndef HAL_SPI_H_
#define HAL_SPI_H_

#include <stdint.h>

/* Opaque handle. Whatever you need: SPI peripheral, CS pin, mutex, ... */
typedef struct hal_spi_dev hal_spi_dev_t;

hal_spi_dev_t *hal_spi_init(void);
/**
 * Take exclusive access to the device (typically an RTOS mutex).
 * The driver holds the lock across multi-frame operations (read = 2 frames,
 * read-modify-write = 3 frames), and never locks recursively.
 *
 * @return 0 on success, negative on error.
 */
int hal_spi_lock(hal_spi_dev_t *dev);

/** Release exclusive access taken by hal_spi_lock(). */
void hal_spi_unlock(hal_spi_dev_t *dev);

/**
 * Perform ONE 16-bit full-duplex frame: assert CS, clock out tx (MSB first),
 * clock in rx, deassert CS.
 *
 * @param tx  Word to send.
 * @param rx  Received word. May be NULL if the reply is not needed.
 * @return 0 on success, negative on error.
 */
int hal_spi_transfer16(hal_spi_dev_t *dev, uint16_t tx, uint16_t *rx);

#endif /* HAL_SPI_H_ */
