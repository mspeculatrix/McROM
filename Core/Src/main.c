/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : McROM STM32H523CET6
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
#define ROM_SIZE_BYTES 0x4000     // Our ROM image is 16KB
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// These are labels defined in the ROM binary object rom.o
extern uint8_t _binary_rom_bin_start[];
extern uint8_t _binary_rom_bin_end[];
extern uint8_t _binary_rom_bin_size;

// Create an array to hold the ROM data. We'll be indexing into this
// array to fetch the byte appropriate to a given address.
uint8_t rom_ram[ROM_SIZE_BYTES] __attribute__((aligned(4)));

// Cached MODER values
uint32_t MODER_A_OUT, MODER_A_IN;
uint32_t MODER_B_OUT, MODER_B_IN;

// Fast Data Bus LUTs - they store BSRR values for atomic port writes
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
// Prototype of the function that provides the main loop
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
  /*****************************************************************************
  *****  SETUP                                                             *****
  *****************************************************************************/
  /* USER CODE END 1 */

  MPU_Config();
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ICACHE_Init();
  
  /* USER CODE BEGIN 2 */
  
  // Prepare RAM
  // The actual ROM size was defined in the binary object rom.o
  // as _binary_rom_bin_size
  uintptr_t actual_rom_size = (uintptr_t)&_binary_rom_bin_size;
  // Fill our array with 0s
  memset(rom_ram, 0, ROM_SIZE_BYTES);
  // We'll now copy the ROM data that was written into flash into our
  // RAM-resident array, because it's faster to serve data from there.
  if (actual_rom_size > 0) {
    uint32_t copy_limit = (actual_rom_size > ROM_SIZE_BYTES) 
      ? ROM_SIZE_BYTES : (uint32_t)actual_rom_size;
    memcpy(rom_ram, _binary_rom_bin_start, copy_limit);
  } else {
    // Something went wrong, so let's fill the aray with NOP opcodes
    memset(rom_ram, 0xEA, ROM_SIZE_BYTES); // NOP Fill
  }

  // Pre-calculate MODER states
  MODER_A_IN  = GPIOA->MODER & ~0x03FF0000; // Clear PA8-PA12
  MODER_A_OUT = MODER_A_IN  | 0x01550000;  // Set PA8-PA12 to Output
  MODER_B_IN  = GPIOB->MODER & ~0xFC000000; // Clear PB13-PB15
  MODER_B_OUT = MODER_B_IN  | 0x54000000;  // Set PB13-PB15 to Output

  // Pre-calculate BSRR LUTs for the Data Bus
  // Data: D0-D2 (PB13-15), D3-D7 (PA8-12)
  for(int i=0; i<256; i++) {
      // Port A: D3-D7 are bits 3-7 of 'i'. Shift to PA8-12.
      // Reset mask is top 16 bits, Set mask is bottom 16 bits.
      lut_data_PA[i] = (0x1F00 << 16) | ((i & 0xF8) << 5);
      
      // Port B: D0-D2 are bits 0-2 of 'i'. Shift to PB13-15.
      lut_data_PB[i] = (0xE000 << 16) | ((i & 0x07) << 13);
  }

  // Set up some pointers to the relevant ports
  GPIO_TypeDef *pA = GPIOA;
  GPIO_TypeDef *pB = GPIOB;
  GPIO_TypeDef *pC = GPIOC;

  // Turn off stuff that might mess with timing/speed.
  HAL_SuspendTick(); 
  __disable_irq();

  // Call the function that provides the main loop. This runs in ITCM RAM.
  ROM_Emulator_Loop(pA, pB, pC);

  // We shouldn't get any further than this.
  /* USER CODE END 2 */

  while (1) {

  /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
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

/* USER CODE BEGIN 4 */
/**
  * @brief Core emulator loop, running in ITCM RAM
  * Shift-and-mask address reconstruction & BSRR LUT data drive.
  */
__attribute__((section(".RamFunc"), noinline))
void ROM_Emulator_Loop(GPIO_TypeDef *pA, GPIO_TypeDef *pB, GPIO_TypeDef *pC) {
  /* SYNC GUARD: Wait for /ROM_ENABLE to be HIGH before starting.
  This prevents the STM32 from catching a partial cycle or 
  power-on glitch during the 6502's reset phase. */
  // while (!(pA->IDR & (1 << 15)));

    while (1) {

    // Wait for Chip Enable (/ROM_ENABLE) on pin 15 to go LOW
    while (pA->IDR & (1 << 15)); 
    
    // A very tiny delay to let the address bus stabilize
    // 6502 addresses can have "skew" where some bits arrive slightly later.
    __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");

    // We did previously wait for the PHI2-qualified /OE (/READ_EN)
    // to be low. But adding this broke things.
    // while (pB->IDR & (1 << 6)); 

    // Sample all address ports immediately to get a snapshot of their state.
    uint32_t rA = pA->IDR; 
    uint32_t rB = pB->IDR; 
    uint32_t rC = pC->IDR;

    // Reconstruct the address on the address bus using the bit we need from
    // the snapshot.
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

    // Get the data byte corresponding to the address from the ROM data array.
    uint8_t val = rom_ram[addr];

    // Pre-stage data on the Data Bus using BSRR (Atomic)
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

    // Wait for /OE (/READ_EN) to go HIGH (End of cycle)
    while (!(pB->IDR & (1 << 6)));

    // Return Data Bus pins to High-Z by setting as inputs.
    pA->MODER = MODER_A_IN;
    pB->MODER = MODER_B_IN;
  }
}
/* USER CODE END 4 */

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
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

