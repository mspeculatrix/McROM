/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : McROM Ultimate Edition - Optimized for 65C02 Timing
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "icache.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ROM_SIZE_BYTES 0x4000  // 16KB
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern uint8_t _binary_rom_bin_start[];
extern uint8_t _binary_rom_bin_end[];
extern uint8_t _binary_rom_bin_size;

uint8_t rom_ram[ROM_SIZE_BYTES] __attribute__((aligned(4)));

/* Cached MODER values */
uint32_t MODER_A_OUT, MODER_A_IN;
uint32_t MODER_B_OUT, MODER_B_IN;

/* Fast Data Bus LUTs - Stores BSRR values for atomic port writes */
uint32_t lut_data_PA[256];
uint32_t lut_data_PB[256];

/* ***DEBUG*** Debugging Buffer */
#define LOG_SIZE 256
uint16_t debug_addr_log[LOG_SIZE];
uint8_t  debug_val_log[LOG_SIZE];
volatile uint8_t debug_idx = 0;
volatile uint8_t log_enabled = 1; 

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void ROM_Emulator_Loop(GPIO_TypeDef *pA, 
  GPIO_TypeDef *pB, 
  GPIO_TypeDef *pC) __attribute__((section(".RamFunc"), noinline));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  MPU_Config();
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ICACHE_Init();
  
  /* USER CODE BEGIN 2 */
  
  // Prepare RAM
  uintptr_t actual_rom_size = (uintptr_t)&_binary_rom_bin_size;
  memset(rom_ram, 0, ROM_SIZE_BYTES);
  if (actual_rom_size > 0) {
      uint32_t copy_limit = (actual_rom_size > ROM_SIZE_BYTES) ? ROM_SIZE_BYTES : (uint32_t)actual_rom_size;
      memcpy(rom_ram, _binary_rom_bin_start, copy_limit);
  } else {
      memset(rom_ram, 0xEA, ROM_SIZE_BYTES); // NOP Fill
  }

  // Pre-calculate MODER states
  MODER_A_IN  = GPIOA->MODER & ~0x03FF0000; // Clear PA8-PA12
  MODER_A_OUT = MODER_A_IN  | 0x01550000;  // Set PA8-PA12 to Output
  MODER_B_IN  = GPIOB->MODER & ~0xFC000000; // Clear PB13-PB15
  MODER_B_OUT = MODER_B_IN  | 0x54000000;  // Set PB13-PB15 to Output

  // Pre-calculate BSRR LUTs for Data Bus
  // Data: D0-D2 (PB13-15), D3-D7 (PA8-12)
  for(int i=0; i<256; i++) {
      // Port A: D3-D7 are bits 3-7 of 'i'. Shift to PA8-12.
      // Reset mask is top 16 bits, Set mask is bottom 16 bits.
      lut_data_PA[i] = (0x1F00 << 16) | ((i & 0xF8) << 5);
      
      // Port B: D0-D2 are bits 0-2 of 'i'. Shift to PB13-15.
      lut_data_PB[i] = (0xE000 << 16) | ((i & 0x07) << 13);
  }

  GPIO_TypeDef *pA = GPIOA;
  GPIO_TypeDef *pB = GPIOB;
  GPIO_TypeDef *pC = GPIOC;

  HAL_SuspendTick(); 
  __disable_irq();

  ROM_Emulator_Loop(pA, pB, pC);
  /* USER CODE END 2 */

  while (1) {}
}

/**
  * @brief Core emulator loop, running in ITCM RAM
  * Shift-and-mask address reconstruction & BSRR LUT data drive.
  */
__attribute__((section(".RamFunc"), noinline))
void ROM_Emulator_Loop(GPIO_TypeDef *pA, GPIO_TypeDef *pB, GPIO_TypeDef *pC) {
  while (1) {
    // Wait for Chip Enable (/ROM_ENABLE) on pin 15 to go LOW
    while (pA->IDR & (1 << 15)); 
    
    // We did previously wait for the PHI2-qualified /OE (/READ_EN)
    // to be low. But adding this broke things.
    // while (pB->IDR & (1 << 6)); 

    // Sample all address ports immediately
    uint32_t rA = pA->IDR; 
    uint32_t rB = pB->IDR; 
    uint32_t rC = pC->IDR;

    // Optimised address reconstruction
    // Mapping: A0:PB10, A1:PB2, A2:PB1, A3:PB0, A4:PA7, A5:PA6, A6:PA3, A7:PA2
    //          A8:PC14, A9:PC13, A10:PB4, A11:PB7, A12:PA1, A13:PC15
    uint16_t addr = ((rB >> 10) & 0x0001) | // A0
                    ((rB >> 1)  & 0x0002) | // A1
                    ((rB << 1)  & 0x0004) | // A2
                    ((rB << 3)  & 0x0008) | // A3
                    ((rA >> 3)  & 0x0010) | // A4
                    ((rA >> 1)  & 0x0020) | // A5
                    ((rA << 3)  & 0x0040) | // A6
                    ((rA << 5)  & 0x0080) | // A7
                    ((rC >> 6)  & 0x0100) | // A8
                    ((rC >> 4)  & 0x0200) | // A9
                    ((rB << 6)  & 0x0400) | // A10
                    ((rB << 4)  & 0x0800) | // A11
                    ((rA << 11) & 0x1000) | // A12
                    ((rC >> 2)  & 0x2000);  // A13

    uint8_t val = rom_ram[addr];

    // Pre-stage data using BSRR (Atomic)
    pA->BSRR = lut_data_PA[val];
    pB->BSRR = lut_data_PB[val];

    // Drive the Data Bus
    pA->MODER = MODER_A_OUT;
    pB->MODER = MODER_B_OUT;

    // ***DEBUG*** (After Bus Drive to protect timing)
    // if (log_enabled) {
    //   debug_addr_log[debug_idx] = addr;
    //   debug_val_log[debug_idx] = val;
    //   debug_idx++; 
    // }

    // F. Wait for /READ_EN (PB6) to go High (End of cycle)
    while (!(pB->IDR & (1 << 6)));

    // G. High-Z
    pA->MODER = MODER_A_IN;
    pB->MODER = MODER_B_IN;
  }
}

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

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

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
