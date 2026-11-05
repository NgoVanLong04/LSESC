#ifndef HAL_ADC
#define HAL_ADC

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "main.h"

/* ===== Hằng số (phải nằm trong header vì hàm inline cần chúng) ===== */
#define RSHUNT_VALUE    0.0005f
#define OPAMP_GAIN      20.0f
#define VREF            3.3f
#define ADC_RESOLUTION  4096.0f
#define VOL_OFFSET      1.65f
#define ADC_OFFSET      2048
#define RAW_TO_VOLS     (VREF / ADC_RESOLUTION)
#define ADC_TO_AMPS     (RAW_TO_VOLS / (RSHUNT_VALUE * OPAMP_GAIN))

#define RH_VBUS         39000
#define RL_VBUS         2200
#define RH_FET          39000
#define RL_FET          2200

HAL_StatusTypeDef adc_init(uint32_t* raw_vbus);

/* ===== Định nghĩa đầy đủ (thân hàm) ngay trong header ===== */
static inline float adc_to_amps(uint16_t raw_adc)
{
    return ((int32_t)raw_adc - ADC_OFFSET) * ADC_TO_AMPS;
}

static inline float convert_to_vol(uint16_t raw_adc, uint32_t res_high, uint32_t res_low)
{
    float por = (float)(res_high + res_low) / res_low;
    return raw_adc * por * RAW_TO_VOLS;
}

static inline float adc_to_vbus(uint16_t raw_adc)
{
    return convert_to_vol(raw_adc, RH_VBUS, RL_VBUS);   /* ← có return */
}

static inline float adc_to_vfet(uint16_t raw_adc)
{
    return convert_to_vol(raw_adc, RH_FET, RL_FET);     /* ← có return */
}

#ifdef __cplusplus
}
#endif

#endif
