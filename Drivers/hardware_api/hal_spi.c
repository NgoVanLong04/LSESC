/*
 * hal_spi.c - STM32 HAL + FreeRTOS implementation of hal_spi.h
 *
 * This is the only file that knows about the platform. To port the driver,
 * rewrite this file and keep hal_spi.h unchanged.
 *
 * Required CubeMX configuration for the DRV8301 SPI:
 *   - Data size  : 16 bits
 *   - First bit  : MSB first
 *   - CPOL = Low, CPHA = 2 Edge (SPI mode 1, check against the datasheet)
 *   - NSS        : Software (CS is a plain GPIO output, default high)
 *   - Baud rate  : SCK <= 10 MHz
 */

#include "FreeRTOS.h"
#include "semphr.h"

#include "hal_spi.h"
#include "spi.h"
#include "main.h"

struct hal_spi_dev {
	SPI_HandleTypeDef *hspi;
	GPIO_TypeDef *cs_port;
	uint16_t cs_pin;
};

static struct hal_spi_dev s_drv8301 = {
	.hspi = &hspi3,
	.cs_port = SPI3_CS_GPIO_Port,
	.cs_pin = SPI3_CS_Pin,
};

/* One mutex per bus (SPI3). If another device shares the bus, share this mutex. */
static SemaphoreHandle_t s_mutex;

/**
 * Create the mutex and return the device handle to pass to drv8301_init().
 * Call once before using the driver, from a task or before the scheduler starts.
 */
hal_spi_dev_t *hal_spi_init(void) {
	if (s_mutex == NULL) {
		s_mutex = xSemaphoreCreateMutex();
	}

	if (s_mutex == NULL) {
		return NULL;
	}

	HAL_GPIO_WritePin(s_drv8301.cs_port, s_drv8301.cs_pin, GPIO_PIN_SET);
	return &s_drv8301;
}

int hal_spi_lock(hal_spi_dev_t *d) {
	(void)d;
	return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE ? 0 : -1;
}

void hal_spi_unlock(hal_spi_dev_t *d) {
	(void)d;
	xSemaphoreGive(s_mutex);
}

int hal_spi_transfer16(hal_spi_dev_t *d, uint16_t tx, uint16_t *rx) {
	uint16_t tmp = 0;

	HAL_GPIO_WritePin(d->cs_port, d->cs_pin, GPIO_PIN_RESET);
	/* Size is the number of 16-bit items (hspi is configured for 16-bit data) */
	HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(d->hspi,
			(uint8_t *)&tx, (uint8_t *)&tmp, 1, 100);
	HAL_GPIO_WritePin(d->cs_port, d->cs_pin, GPIO_PIN_SET);

	if (st != HAL_OK) {
		return -1;
	}

	if (rx != NULL) {
		*rx = tmp;
	}

	return 0;
}
