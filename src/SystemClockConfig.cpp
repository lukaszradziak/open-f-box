#include <stm32f1xx_hal.h>

/* The stock STM32duino variant for STM32F105RBT6 provides an empty weak
 * SystemClock_Config(). This board has an 8 MHz HSE crystal. Configure a
 * 72 MHz system clock and the 48 MHz clock required by USB OTG FS.
 */
extern "C" void SystemClock_Config(void) {
    RCC_OscInitTypeDef oscInit = {0};
    RCC_ClkInitTypeDef clkInit = {0};
    RCC_PeriphCLKInitTypeDef periphInit = {0};

    oscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscInit.Prediv1Source = RCC_PREDIV1_SOURCE_HSE;
    oscInit.HSEState = RCC_HSE_ON;
    oscInit.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscInit.PLL.PLLState = RCC_PLL_ON;
    oscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscInit.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&oscInit) != HAL_OK) return;

    clkInit.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                        RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clkInit.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clkInit.APB1CLKDivider = RCC_HCLK_DIV2;
    clkInit.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clkInit, FLASH_LATENCY_2) != HAL_OK) return;

    // On STM32F105/F107 OTGFSPRE=0 selects PLLCLK/1.5: 72/1.5 = 48 MHz.
    periphInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    periphInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV3;
    HAL_RCCEx_PeriphCLKConfig(&periphInit);
}
