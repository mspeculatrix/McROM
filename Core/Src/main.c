/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : McROM Ultimate Edition - ITCM RAM Resident Emulator
  ******************************************************************************

  Summary of the McROM build:

   - Hardware: STM32H523CET6 connected via a custom 14-bit address & 8-bit 
     data mapping.

   - Storage: ROM data is baked into Flash at 0x08060000 via objcopy.

   - Execution: Core logic runs from SRAM (0x20000440) for deterministic, 
     zero-wait-state performance.

   - Speed: Optimized with MODER pre-caching and Direct Register Access.

  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "icache.h"
#include "gpio.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Linker symbols from objcopy */
extern const uint8_t _binary_rom_bin_start[];
extern const uint8_t _binary_rom_bin_end[];

/* 16KB RAM buffer aligned for 32-bit access speed */
uint8_t rom_ram[0x4000] __attribute__((aligned(4)));

/* Cached MODER values for high-speed switching */
uint32_t MODER_A_OUT, MODER_A_IN;
uint32_t MODER_B_OUT, MODER_B_IN;

/* Data bus masks */
#define PA_DATA_MASK   (0x1F00)       
#define PB_DATA_MASK   (0xE000)       

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);

/* USER CODE BEGIN PFP */

/* Function prototype for the RAM-resident loop */
void ROM_Emulator_Loop(GPIO_TypeDef *pA, GPIO_TypeDef *pB, GPIO_TypeDef *pC) __attribute__((section(".RamFunc"), noinline));

/* USER CODE END PFP */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* MCU Configuration */
  MPU_Config();
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  
  /* USER CODE BEGIN 2 */
  
  // Load ROM code into STM32 RAM
  uint32_t rom_size = (uint32_t)_binary_rom_bin_end - (uint32_t)_binary_rom_bin_start;
  memset(rom_ram, 0, sizeof(rom_ram));
  if (rom_size > 0) {
      uint32_t copy_limit = (rom_size > 0x4000) ? 0x4000 : rom_size;
      memcpy(rom_ram, _binary_rom_bin_start, copy_limit);
  } else {
      rom_ram[0] = 0xEE; // Error pattern for bug analysis
  }

  // Pre-calculate MODER states to save cycles in the loop
  MODER_A_IN  = GPIOA->MODER & ~0x03FF0000; 
  MODER_A_OUT = MODER_A_IN  | 0x01550000;  
  MODER_B_IN  = GPIOB->MODER & ~0xFC000000; 
  MODER_B_OUT = MODER_B_IN  | 0x54000000;  

  GPIO_TypeDef *pA = GPIOA;
  GPIO_TypeDef *pB = GPIOB;
  GPIO_TypeDef *pC = GPIOC;

  // Disable interrupts and start the emulator
  HAL_SuspendTick(); 
  __disable_irq();

  // Jump to ITCM RAM loop - this is where the main code actually executes
  ROM_Emulator_Loop(pA, pB, pC);

  /* USER CODE END 2 */
  while (1) {} // We should never reach here
}

/* USER CODE BEGIN 4 */
/**
  * @brief Core emulator loop, running in ITCM RAM for zero wait-states
  */
__attribute__((section(".RamFunc"), noinline))
void ROM_Emulator_Loop(GPIO_TypeDef *pA, GPIO_TypeDef *pB, GPIO_TypeDef *pC) {
    while (1) {
        // Wait for /ROM_ENABLE (PA15) LOW
        while (pA->IDR & (1 << 15)); 

        // Signal Settling (Adjust NOPs for your specific bus speed/ringing)
        __asm__("nop"); __asm__("nop"); __asm__("nop");

        // Snapshot Address Bus
        uint32_t rA = pA->IDR;
        uint32_t rB = pB->IDR;
        uint32_t rC = pC->IDR;

        // Reconstruct Address
        uint16_t addr = 0;
        if (rB & (1 << 10)) addr |= (1 << 0);
        if (rB & (1 << 2))  addr |= (1 << 1);
        if (rB & (1 << 1))  addr |= (1 << 2);
        if (rB & (1 << 0))  addr |= (1 << 3);
        if (rA & (1 << 7))  addr |= (1 << 4);
        if (rA & (1 << 6))  addr |= (1 << 5);
        if (rA & (1 << 3))  addr |= (1 << 6);
        if (rA & (1 << 2))  addr |= (1 << 7);
        if (rC & (1 << 14)) addr |= (1 << 8);
        if (rC & (1 << 13)) addr |= (1 << 9);
        if (rB & (1 << 4))  addr |= (1 << 10);
        if (rB & (1 << 7))  addr |= (1 << 11);
        if (rA & (1 << 1))  addr |= (1 << 12);
        if (rC & (1 << 15)) addr |= (1 << 13);

        uint8_t val = rom_ram[addr & 0x3FFF];

        // Drive Data Bus (ODR then MODER)
        pB->ODR = (pB->ODR & ~PB_DATA_MASK) | ((val & 0x07) << 13);
        pA->ODR = (pA->ODR & ~PA_DATA_MASK) | ((val & 0xF8) << 5);
        
        pA->MODER = MODER_A_OUT;
        pB->MODER = MODER_B_OUT;

        // Wait for /ROM_ENABLE HIGH
        while (!(pA->IDR & (1 << 15)));

        // Release Bus (High-Z)
        pA->MODER = MODER_A_IN;
        pB->MODER = MODER_B_IN;
    }
}
/* USER CODE END 4 */

/**
  * @brief System Clock Configuration (250MHz)
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 125;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* MPU Configuration */
void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

  HAL_MPU_Disable();
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08FFF000;
  MPU_InitStruct.LimitAddress = 0x08FFFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  This function is executed when errors occur.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
