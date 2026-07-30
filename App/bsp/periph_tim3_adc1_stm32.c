/*
 * TIM3 (buzzer PWM, PB5/CH2) and ADC1 (battery sense, PC0/IN10) bring-up.
 *
 * These two peripherals are owned HERE rather than by CubeMX: headless
 * CubeMX 6.15 silently drops hand-added ADC/TIM blocks when it migrates the
 * .ioc on `config load`, so the generated Core/ contains no MX_TIM3_Init /
 * MX_ADC1_Init, and its gpio.c puts PB5 in AF mode without ever setting the
 * Alternate field. Owning the init in App/ keeps it versioned and immune to
 * regeneration. If the project is later opened in the CubeMX GUI and both
 * peripherals are enabled there properly, delete this file and let Core/
 * define the handles again.
 *
 * Must run from hk_app_init() BEFORE any driver touches htim3 / hadc1.
 */
#if !defined(HK_HOST)

#include "bsp/board_config.h"

/* Storage for the handles board_config.h declares extern. */
TIM_HandleTypeDef htim3;
ADC_HandleTypeDef hadc1;

hk_status_t hk_bsp_tim3_adc1_init(void)
{
    GPIO_InitTypeDef io = {0};

    /* --- TIM3_CH2 buzzer PWM on PB5 (AF2) ---------------------------- */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    io.Pin       = GPIO_PIN_5;
    io.Mode      = GPIO_MODE_AF_PP;
    io.Pull      = GPIO_NOPULL;
    io.Speed     = GPIO_SPEED_FREQ_LOW;
    io.Alternate = GPIO_AF2_TIM3;      /* gpio.c leaves this unset */
    HAL_GPIO_Init(GPIOB, &io);

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;  /* buzzer driver reprograms per tone */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 999;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        return HK_ERR_IO;
    }

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, HK_BUZZER_TIM_CHANNEL) != HAL_OK) {
        return HK_ERR_IO;
    }

    /* --- ADC1_IN10 battery sense on PC0 ------------------------------ */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    io.Pin       = GPIO_PIN_0;
    io.Mode      = GPIO_MODE_ANALOG;
    io.Pull      = GPIO_NOPULL;
    io.Alternate = 0;
    HAL_GPIO_Init(GPIOC, &io);

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        return HK_ERR_IO;
    }

    /* High source impedance behind the 100k/10k divider => longest sample
     * time. The battery task reads ~1 Hz, so speed is irrelevant. */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = HK_BAT_ADC_CHANNEL;
    ch.Rank         = 1;
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) {
        return HK_ERR_IO;
    }

    return HK_OK;
}

#endif /* !HK_HOST */
