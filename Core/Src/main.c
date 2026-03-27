/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : McROM Ultimate Edition - ITCM RAM Resident Emulator
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

/* Linker-generated symbols for the binary data */
extern uint8_t _binary_rom_bin_start[];
extern uint8_t _binary_rom_bin_end[];
extern uint8_t _binary_rom_bin_size; // Address of this symbol is the size

/* The actual buffer used by the emulator loop */
volatile uint8_t rom_ram[ROM_SIZE_BYTES] __attribute__((aligned(4)));

/* Cached MODER values for high-speed switching */
uint32_t MODER_A_OUT, MODER_A_IN;
uint32_t MODER_B_OUT, MODER_B_IN;

/* Data bus masks */
#define PA_DATA_MASK   (0x1F00)       
#define PB_DATA_MASK   (0xE000)

/* Debugging Buffer */
uint16_t debug_addr_log[256];
uint8_t  debug_val_log[256];
volatile uint8_t debug_idx = 0;
volatile uint8_t log_enabled = 1;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void ROM_Emulator_Loop(GPIO_TypeDef *pA, GPIO_TypeDef *pB, GPIO_TypeDef *pC) __attribute__((section(".RamFunc"), noinline));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  
  /* USER CODE BEGIN 2 */
  
  // Calculate size from linker symbols
  uintptr_t actual_rom_size = (uintptr_t)&_binary_rom_bin_size;
  
  // Prepare the RAM buffer
  memset(rom_ram, 0, ROM_SIZE_BYTES);
  
  if (actual_rom_size > 0) {
      uint32_t copy_limit = (actual_rom_size > ROM_SIZE_BYTES) ? ROM_SIZE_BYTES : (uint32_t)actual_rom_size;
      memcpy(rom_ram, _binary_rom_bin_start, copy_limit);
  } else {
      // Fallback: Fill with 6502 NOPs ($EA) if binary is missing
      memset(rom_ram, 0xEA, ROM_SIZE_BYTES);
  }

  // Pre-calculate MODER states
  // Ensure we only touch the bits for our data pins
  MODER_A_IN  = GPIOA->MODER & ~0x03FF0000; 
  MODER_A_OUT = MODER_A_IN  | 0x01550000;  
  MODER_B_IN  = GPIOB->MODER & ~0xFC000000; 
  MODER_B_OUT = MODER_B_IN  | 0x54000000;  

  GPIO_TypeDef *pA = GPIOA;
  GPIO_TypeDef *pB = GPIOB;
  GPIO_TypeDef *pC = GPIOC;

  // Final sanity check: If Reset Vector is $0000, force it to $C000 for testing
  // if (rom_ram[0x3FFC] == 0 && rom_ram[0x3FFD] == 0) {
  //     rom_ram[0x3FFC] = 0x00; 
  //     rom_ram[0x3FFD] = 0xC0;
  // }

  // Go dark: Disable interrupts and system heartbeats
  HAL_SuspendTick(); 
  __disable_irq();

  // Enter the high-speed loop in RAM. This is where all the action happens.
  ROM_Emulator_Loop(pA, pB, pC);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  // We're not using the traditional main loop and should never get here.
  // Not entirely sure why I'm leaving this in place but maybe
  // STM32CubeMX prefers it this way.
  while (1)
  {
    /* USER CODE END WHILE */

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
  * Optimized for 65C02 PHI2 timing.
  */
__attribute__((section(".RamFunc"), noinline))
void ROM_Emulator_Loop(GPIO_TypeDef *pA, GPIO_TypeDef *pB, GPIO_TypeDef *pC) {
    while (1) {
        // Wait for /ROM_ENABLE (PA15 / CE) to be LOW
        while (pA->IDR & (1 << 15)); 

        // Snapshot Address immediately
        uint32_t rA = pA->IDR;
        uint32_t rB = pB->IDR;
        uint32_t rC = pC->IDR;

        // Reconstruct Address (Early calculation while PHI2 may still be LOW)
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

        // Prime the ODR (pins are still in input mode, so no bus fight)
        pB->ODR = (pB->ODR & ~PB_DATA_MASK) | ((val & 0x07) << 13);
        pA->ODR = (pA->ODR & ~PA_DATA_MASK) | ((val & 0xF8) << 5);

        // TRIGGER: Wait for Output Enable / PHI2 (PB6) to be LOW
        while (pB->IDR & (1 << 6));

        // DRIVE: Instant switch to Output mode
        pA->MODER = MODER_A_OUT;
        pB->MODER = MODER_B_OUT;

        // HOLD: Wait for the cycle to end (/CE goes HIGH or PHI2 goes LOW/OE goes HIGH)
        while (!(pA->IDR & (1 << 15)) && !(pB->IDR & (1 << 6)));

        // RELEASE: Immediate High-Z
        pA->MODER = MODER_A_IN;
        pB->MODER = MODER_B_IN;
    }
}
/* USER CODE END 4 */

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
