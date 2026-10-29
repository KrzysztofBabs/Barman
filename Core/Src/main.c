/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "libjpeg.h"
#include "app_touchgfx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define REFRESH_COUNT        1835

#define SDRAM_TIMEOUT                            ((uint32_t)0xFFFF)
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

#define FLASH_READ_JEDEC_ID				0x9F
#define FLASH_FAST_READ_QUAD_IO		 	0xEB
#define FLASH_REVC03_MANUFACTURER_ID	0xEF
#define FLASH_N25Q128A_DUMMY_CYCLES		0x0A
#define FLASH_W25Q128J_DUMMY_CYCLES		0x06

// --- Kalibracja wagi (HX711) ---
// HX711_SCALE = ile "kroków" surowej wartosci HX711 przypada na 1 gram.
// Kalibracja: przy skali 61.9 telefon 203g pokazywal ~274, wiec
// skala = 61.9 * 274/203 ≈ 83.5.
#define HX711_SCALE   83.5f

// --- Zawartosc butelek ustawiana z aplikacji ---
#define BOTTLE_COUNT      4
#define BOTTLE_NAME_MAX   16   // bez koncowego '\0'
#define BLE_LINE_MAX      40   // "SET,n," + nazwa, z zapasem
#define BLE_TX_MAX        128  // odpowiedz na $GET: 4 x "$B,n,nazwa\n" + "$END\n"

// --- Zapis nazw butelek w wewnetrznym flashu ---
// Ostatni sektor (7, 256 KB). Linker ma FLASH = 768K, zeby program tu nie wszedl.
#define NAMES_FLASH_ADDR    0x080C0000u
#define NAMES_FLASH_SIZE    0x40000u
#define NAMES_FLASH_SECTOR  FLASH_SECTOR_7
#define NAMES_MAGIC         0x424F544Cu  // "BOTL"
#define NAMES_SAVE_DELAY_MS 2000u        // zapis 2 s po ostatniej zmianie

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

CRC_HandleTypeDef hcrc;

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c3;

LTDC_HandleTypeDef hltdc;

QSPI_HandleTypeDef hqspi;

SDRAM_HandleTypeDef hsdram1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TouchGFXTask */
osThreadId_t TouchGFXTaskHandle;
const osThreadAttr_t TouchGFXTask_attributes = {
  .name = "TouchGFXTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for videoTask */
osThreadId_t videoTaskHandle;
const osThreadAttr_t videoTask_attributes = {
  .name = "videoTask",
  .stack_size = 1000 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE BEGIN PV */
static FMC_SDRAM_CommandTypeDef Command;

UART_HandleTypeDef huart6;
uint8_t ble_rx_data = 0;
// Komenda z BLE czekajaca na wykonanie przez ekran (0 = brak).
static volatile uint8_t ble_command = 0;
// 1 = poprzedni bajt byl '#', wiec ten bajt jest komenda.
static uint8_t ble_prefix_seen = 0;

// Wiadomosc tekstowa "$...\n" zbierana bajt po bajcie (bez '$' i '\n').
static char ble_line[BLE_LINE_MAX + 1];
static uint8_t ble_line_len = 0;
static uint8_t ble_line_active = 0;
static uint8_t ble_line_overflow = 0;

// Bufor odpowiedzi - musi zyc do konca wysylki przez HAL_UART_Transmit_IT.
static char ble_tx_buf[BLE_TX_MAX];
// Tu skladana jest odpowiedz, zanim BLE_Send skopiuje ja do ble_tx_buf.
static char ble_reply[BLE_TX_MAX];

// Co jest w ktorej butelce (ustawiane z aplikacji komenda $SET).
static char bottleName[BOTTLE_COUNT][BOTTLE_NAME_MAX + 1] = {

  "null", "null", "null", "null"
    // "Butelka 1", "Butelka 2", "Butelka 3", "Butelka 4"
};
// Zwiekszany przy kazdej zmianie nazwy - ekran po nim poznaje, ze trzeba odswiezyc.
static volatile uint8_t bottleNamesVersion = 0;

// Nazwy czekajace na zapis do flasha i czas ostatniej zmiany.
static volatile uint8_t bottleNamesDirty = 0;
static volatile uint32_t bottleNamesChangedTick = 0;

// Sektor jest zapisywany kolejnymi rekordami jak dziennik - kasowanie tylko
// gdy sie zapelni (ok. 3600 zapisow). Waznym rekordem jest ostatni z magic.
// magic jest na koncu i programowany jako ostatni, wiec rekord przerwany
// odlaczeniem zasilania nie ma magic i jest pomijany.
typedef struct
{
    char names[BOTTLE_COUNT][BOTTLE_NAME_MAX + 1];
    uint32_t magic;
} BottleNamesRecord;

_Static_assert(sizeof(BottleNamesRecord) % 4 == 0, "rekord programowany slowami 32-bit");

#define NAMES_SLOT_COUNT  (NAMES_FLASH_SIZE / sizeof(BottleNamesRecord))

// Pierwszy wolny rekord w sektorze (NAMES_SLOT_COUNT = sektor pelny).
static uint32_t namesNextSlot = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_DMA2D_Init(void);
static void MX_FMC_Init(void);
static void MX_I2C3_Init(void);
static void MX_LTDC_Init(void);
static void MX_QUADSPI_Init(void);
void StartDefaultTask(void *argument);
extern void TouchGFX_Task(void *argument);
extern void videoTaskFunc(void *argument);

/* USER CODE BEGIN PFP */
void GetManufacturerId (uint8_t *manufacturer_id);
void EnableMemoryMappedMode(uint8_t manufacturer_id);  
static void MX_USART6_UART_Init(void);
static void BottleNames_Load(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int32_t waga_surowa = 0;      // surowa wartosc z HX711 (przydatna do kalibracji)
int32_t waga_gramy  = 0;      // przeliczona masa w gramach (to wyswietlamy na ekranie)
static int32_t hx711_offset = 0;  // tara - odczyt przy pustej belce

// Pojedynczy odczyt 24-bit z HX711 (kanal A, wzmocnienie 128).
int32_t HX711_Read(void) {
    uint32_t count = 0;
    // Czekamy az DOUT spadnie do 0 = dane gotowe. Z timeoutem, by nie zawiesic
    // sie gdy czujnik nie jest podlaczony.
    uint32_t timeout = HAL_GetTick() + 200;
    while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_SET) {
        if (HAL_GetTick() > timeout) {
            return hx711_offset; // brak danych -> zwroc tare (czyli 0 g)
        }
        osDelay(1);
    }
    for(int i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_SET);
        for(volatile int j=0; j<50; j++);
        count = count << 1;
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);
        for(volatile int j=0; j<50; j++);
        if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_SET) count++;
    }
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_SET);
    for(volatile int j=0; j<50; j++);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);
    if (count & 0x800000) count |= 0xFF000000;
    return (int32_t)count;
}

// Srednia z kilku odczytow - mniej szumu, stabilniejszy wynik.
int32_t HX711_ReadAverage(uint8_t times) {
    int64_t sum = 0;
    for (uint8_t i = 0; i < times; i++) {
        sum += HX711_Read();
        osDelay(5);
    }
    return (int32_t)(sum / times);
}


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

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

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
  MX_CRC_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_I2C3_Init();
  MX_LTDC_Init();
  MX_QUADSPI_Init();
  MX_LIBJPEG_Init();
  MX_TouchGFX_Init();
  /* Call PreOsInit function */
  MX_TouchGFX_PreOSInit();
  /* USER CODE BEGIN 2 */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN 2 */
__HAL_RCC_GPIOI_CLK_ENABLE();

GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 ; // Pin D5 = PI0 (zlacze Arduino, ta rewizja plytki)
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

// Zawor zamkniety na starcie. Otwiera sie dopiero po nacisnieciu "Nalej".
HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_SET);
// HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_RESET); // <- TEST: zawor otwarty od razu po starcie
BottleNames_Load();
MX_USART6_UART_Init();
  HAL_UART_Receive_IT(&huart6, &ble_rx_data, 1);
/* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of TouchGFXTask */
  TouchGFXTaskHandle = osThreadNew(TouchGFX_Task, NULL, &TouchGFXTask_attributes);

  /* creation of videoTask */
  videoTaskHandle = osThreadNew(videoTaskFunc, NULL, &videoTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */
  HAL_Delay(100); //Delay to fix initialization issue on some boards 
  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x20404768;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 40;
  hltdc.Init.VerticalSync = 9;
  hltdc.Init.AccumulatedHBP = 53;
  hltdc.Init.AccumulatedVBP = 11;
  hltdc.Init.AccumulatedActiveW = 533;
  hltdc.Init.AccumulatedActiveH = 283;
  hltdc.Init.TotalWidth = 565;
  hltdc.Init.TotalHeigh = 285;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 480;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 272;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0xC0000000;
  pLayerCfg.ImageWidth = 480;
  pLayerCfg.ImageHeight = 272;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief QUADSPI Initialization Function
  * @param None
  * @retval None
  */
static void MX_QUADSPI_Init(void)
{

  /* USER CODE BEGIN QUADSPI_Init 0 */

  /* USER CODE END QUADSPI_Init 0 */

  /* USER CODE BEGIN QUADSPI_Init 1 */

  /* USER CODE END QUADSPI_Init 1 */
  /* QUADSPI parameter configuration*/
  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 1;
  hqspi.Init.FifoThreshold = 4;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  hqspi.Init.FlashSize = 24;
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_6_CYCLE;
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
  hqspi.Init.FlashID = QSPI_FLASH_ID_1;
  hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init(&hqspi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN QUADSPI_Init 2 */
  uint8_t manufacturer_id;
  GetManufacturerId (&manufacturer_id);
  EnableMemoryMappedMode(manufacturer_id);
  HAL_NVIC_DisableIRQ(QUADSPI_IRQn);
  /* USER CODE END QUADSPI_Init 2 */

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 7;
  SdramTiming.WriteRecoveryTime = 3;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */
  __IO uint32_t tmpmrd = 0;

    /* Step 1: Configure a clock configuration enable command */
    Command.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
    Command.CommandTarget          =  FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber      = 1;
    Command.ModeRegisterDefinition = 0;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 2: Insert 100 us minimum delay */
    /* Inserted delay is equal to 1 ms due to systick time base unit (ms) */
    HAL_Delay(1);

    /* Step 3: Configure a PALL (precharge all) command */
    Command.CommandMode            = FMC_SDRAM_CMD_PALL;
    Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber      = 1;
    Command.ModeRegisterDefinition = 0;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 4: Configure an Auto Refresh command */
    Command.CommandMode            = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber      = 8;
    Command.ModeRegisterDefinition = 0;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 5: Program the external memory mode register */
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 | \
             SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL    | \
             SDRAM_MODEREG_CAS_LATENCY_3            | \
             SDRAM_MODEREG_OPERATING_MODE_STANDARD  | \
             SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    Command.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
    Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber      = 1;
    Command.ModeRegisterDefinition = tmpmrd;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 6: Set the refresh rate counter */
    /* Set the device refresh rate */
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, REFRESH_COUNT);

    //Deactivate speculative/cache access to first FMC Bank to save FMC bandwidth
    FMC_Bank1->BTCR[0] = 0x000030D2;
  /* USER CODE END FMC_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(VSYNC_FREQ_GPIO_Port, VSYNC_FREQ_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_3|GPIO_PIN_2|GPIO_PIN_1|LCD_DISP_Pin
                          |GPIO_PIN_0, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, FRAME_RATE_Pin|RENDER_TIME_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MCU_ACTIVE_GPIO_Port, MCU_ACTIVE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : VSYNC_FREQ_Pin */
  GPIO_InitStruct.Pin = VSYNC_FREQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(VSYNC_FREQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PI3 PI2 PI1 LCD_DISP_Pin
                           PI0 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_2|GPIO_PIN_1|LCD_DISP_Pin
                          |GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BL_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : FRAME_RATE_Pin RENDER_TIME_Pin */
  GPIO_InitStruct.Pin = FRAME_RATE_Pin|RENDER_TIME_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : MCU_ACTIVE_Pin */
  GPIO_InitStruct.Pin = MCU_ACTIVE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(MCU_ACTIVE_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// void SetValveState(uint8_t state) {
//     if(state == 1) {
//         HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_RESET); // Otwórz zawór (LOW)
//     } else {
//         HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_SET);   // Zamknij zawór (HIGH)
//     }
// }



void SetValveState(uint8_t valve_id, uint8_t state){
  GPIO_TypeDef* port = GPIOI;
  uint16_t pin = GPIO_PIN_0;

  if(valve_id==1){
    port=GPIOI;
    pin=GPIO_PIN_0;

  }

  else if(valve_id==2){
    port=GPIOI;
    pin=GPIO_PIN_1;
  }

  else if(valve_id==3){
    port=GPIOI;
    pin=GPIO_PIN_2;
  }

  else if(valve_id==4){
    port=GPIOI;
    pin=GPIO_PIN_3;
  }

  if(state==1){
    HAL_GPIO_WritePin(port,pin,GPIO_PIN_RESET);
  }
  else{
    HAL_GPIO_WritePin(port,pin,GPIO_PIN_SET);

  }
}


// Read manufacturer ID of external QSPI flash
void GetManufacturerId (uint8_t *manufacturer_id)
{
  QSPI_CommandTypeDef s_command_id;

  /* Initialize the read flag status register command */
  s_command_id.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command_id.Instruction       = FLASH_READ_JEDEC_ID;
  s_command_id.AddressMode       = QSPI_ADDRESS_NONE;
  s_command_id.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command_id.DataMode          = QSPI_DATA_1_LINE;
  s_command_id.DummyCycles       = 0;
  s_command_id.NbData            = 1;
  s_command_id.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command_id.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command_id.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Configure the command */
  if (HAL_QSPI_Command(&hqspi, &s_command_id, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
	  Error_Handler();
  }

  /* Reception of the data */
  if (HAL_QSPI_Receive(&hqspi, manufacturer_id, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
	  Error_Handler();
  }
}

// Enable memory mapped mode for external QSPI flash
void EnableMemoryMappedMode(uint8_t manufacturer_id)  
{
  QSPI_CommandTypeDef      s_command;
  QSPI_MemoryMappedTypeDef s_mem_mapped_cfg;

  /* Configure the command for the read instruction */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = FLASH_FAST_READ_QUAD_IO;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  
  // Set dummy cycles according to manufacturer ID
  s_command.DummyCycles       = manufacturer_id == FLASH_REVC03_MANUFACTURER_ID ? FLASH_W25Q128J_DUMMY_CYCLES : FLASH_N25Q128A_DUMMY_CYCLES;

  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  
  /* Configure the memory mapped mode */
  s_mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  s_mem_mapped_cfg.TimeOutPeriod     = 0;
  
  if (HAL_QSPI_MemoryMapped(&hqspi, &s_command, &s_mem_mapped_cfg) != HAL_OK)
  {
    Error_Handler();
  }
}

// USART6 na zlaczu Arduino: PC6 = D1 (TX), PC7 = D0 (RX). 9600 8N1.
static void MX_USART6_UART_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = 9600; // domyslna predkosc HM-10 / JDY / HC-05
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        Error_Handler();
    }

    // Priorytet >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5) wymagany przez FreeRTOS.
    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}

// Wysyla odpowiedz do aplikacji bez blokowania (mozna wolac z przerwania).
// Gdy poprzednia odpowiedz jeszcze leci, nowa przepada - aplikacja wysyla
// kolejna komende dopiero po odebraniu odpowiedzi.
static void BLE_Send(const char *text)
{
    if (huart6.gState != HAL_UART_STATE_READY)
    {
        return;
    }
    size_t len = strlen(text);
    if (len > sizeof(ble_tx_buf))
    {
        len = sizeof(ble_tx_buf);
    }
    memcpy(ble_tx_buf, text, len);
    HAL_UART_Transmit_IT(&huart6, (uint8_t *)ble_tx_buf, (uint16_t)len);
}

static uint8_t IsValidBottleName(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len > BOTTLE_NAME_MAX)
    {
        return 0;
    }
    for (size_t i = 0; i < len; i++)
    {
        char ch = name[i];
        if (ch < 0x20 || ch > 0x7E || ch == ',' || ch == '$' || ch == '#')
        {
            return 0;
        }
    }
    return 1;
}

// Dopisuje do ble_reply linie "<prefix>n,Nazwa\n" dla butelki idx (0..3).
static size_t AppendBottleLine(size_t n, const char *prefix, uint8_t idx)
{
    size_t len = strlen(prefix);
    memcpy(&ble_reply[n], prefix, len);   n += len;
    ble_reply[n++] = (char)('1' + idx);
    ble_reply[n++] = ',';
    len = strlen(bottleName[idx]);
    memcpy(&ble_reply[n], bottleName[idx], len);   n += len;
    ble_reply[n++] = '\n';
    ble_reply[n] = '\0';
    return n;
}

// Wiadomosci od aplikacji (w ble_line jest tresc bez '$' i '\n'):
//   "SET,n,Nazwa" -> "$OK,n,Nazwa\n"          - zapisz co jest w butelce n
//   "GET"         -> "$B,1,..\n" .. "$B,4,..\n" "$END\n" - wszystkie butelki
//   "GET,n"       -> "$B,n,Nazwa\n"           - jedna butelka
// Bledy: "$ERR,ID|NAME|CMD|LEN\n".
static void BLE_HandleLine(void)
{
    if (ble_line_overflow)
    {
        BLE_Send("$ERR,LEN\n");
        return;
    }

    if (strcmp(ble_line, "GET") == 0)
    {
        size_t n = 0;
        for (uint8_t i = 0; i < BOTTLE_COUNT; i++)
        {
            n = AppendBottleLine(n, "$B,", i);
        }
        memcpy(&ble_reply[n], "$END\n", 6);
        BLE_Send(ble_reply);
        return;
    }

    if (strncmp(ble_line, "GET,", 4) == 0)
    {
        char id = ble_line[4];
        if (id < '1' || id > ('0' + BOTTLE_COUNT) || ble_line[5] != '\0')
        {
            BLE_Send("$ERR,ID\n");
            return;
        }
        AppendBottleLine(0, "$B,", (uint8_t)(id - '1'));
        BLE_Send(ble_reply);
        return;
    }

    if (strncmp(ble_line, "SET,", 4) != 0)
    {
        BLE_Send("$ERR,CMD\n");
        return;
    }

    char id = ble_line[4];
    char sep = ble_line[5];
    if (id < '1' || id > ('0' + BOTTLE_COUNT) || (sep != ',' && sep != '\0'))
    {
        BLE_Send("$ERR,ID\n");
        return;
    }

    const char *name = (sep == ',') ? &ble_line[6] : "";
    if (!IsValidBottleName(name))
    {
        BLE_Send("$ERR,NAME\n");
        return;
    }

    uint8_t idx = (uint8_t)(id - '1');
    strcpy(bottleName[idx], name);
    bottleNamesVersion++;
    bottleNamesChangedTick = HAL_GetTick();
    bottleNamesDirty = 1;

    AppendBottleLine(0, "$OK,", idx);
    BLE_Send(ble_reply);
}

// Komendy z aplikacji: '#' + jeden znak, np. "#1", "#C", "#S".
//   '#1'..'#4'    - nalej z butelki 1..4 (jak przyciski na ekranie)
//   '#A'..'#C'    - przepis 1..3 (jak wybor z listy + "NALEJ")
//   '#0', '#S'    - STOP, zamyka wszystkie zawory
// Wiadomosci tekstowe: '$' ... '\n', np. "$SET,2,Wodka\n" - co jest w butelce 2.
// Znaki bez '#' lub '$' przed nimi sa ignorowane - HM-10 sam wysyla np. "OK+CONN"
// przy polaczeniu i "OK+LOST" przy rozlaczeniu, co bez prefiksu odpalalo zawory.
// '#' przerywa zbieranie wiadomosci '$', zeby STOP dzialal zawsze.
// Przerwanie tylko zapamietuje komende - wykonuje ja Screen2View, z tymi samymi
// timerami co przy obsludze z ekranu.
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
        uint8_t c = ble_rx_data;

        if (c == '#')
        {
            ble_line_active = 0;
            ble_prefix_seen = 1;
        }
        else if (c == '$')
        {
            ble_prefix_seen = 0;
            ble_line_active = 1;
            ble_line_len = 0;
            ble_line_overflow = 0;
        }
        else if (ble_line_active)
        {
            if (c == '\n')
            {
                ble_line_active = 0;
                ble_line[ble_line_len] = '\0';
                BLE_HandleLine();
            }
            else if (c != '\r')
            {
                if (ble_line_len < BLE_LINE_MAX)
                {
                    ble_line[ble_line_len++] = (char)c;
                }
                else
                {
                    ble_line_overflow = 1;
                }
            }
        }
        else if (ble_prefix_seen)
        {
            ble_prefix_seen = 0;

            switch (c)
            {
                case '1':
                case '2':
                case '3':
                case '4':
                case 'A':
                case 'B':
                case 'C':
                    ble_command = c;
                    break;
                case 'a':
                case 'b':
                case 'c':
                    ble_command = c - 'a' + 'A';
                    break;
                case '0':
                case 's':
                case 'S':
                    // STOP zamyka zawory od razu, nawet gdy otwarty jest inny ekran.
                    SetValveState(1, 0);
                    SetValveState(2, 0);
                    SetValveState(3, 0);
                    SetValveState(4, 0);
                    ble_command = 'S';
                    break;
                default:
                    break;
            }
        }

        HAL_UART_Receive_IT(&huart6, &ble_rx_data, 1);
    }
}

// Np. przepelnienie (overrun) przerywa odbior - bez ponownego uzbrojenia BLE by "zamilkl".
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
        ble_prefix_seen = 0;
        ble_line_active = 0;
        HAL_UART_Receive_IT(&huart6, &ble_rx_data, 1);
    }
}

const char *GetBottleName(uint8_t bottle_id)
{
    if (bottle_id < 1 || bottle_id > BOTTLE_COUNT)
    {
        return "";
    }
    return bottleName[bottle_id - 1];
}

uint8_t GetBottleNamesVersion(void)
{
    return bottleNamesVersion;
}

static const BottleNamesRecord *NamesSlot(uint32_t slot)
{
    return (const BottleNamesRecord *)(NAMES_FLASH_ADDR + slot * sizeof(BottleNamesRecord));
}

static uint8_t IsNamesSlotErased(uint32_t slot)
{
    const uint32_t *word = (const uint32_t *)NamesSlot(slot);
    for (uint32_t i = 0; i < sizeof(BottleNamesRecord) / 4; i++)
    {
        if (word[i] != 0xFFFFFFFFu)
        {
            return 0;
        }
    }
    return 1;
}

// Wczytuje ostatnio zapisane nazwy. Wolac przed startem schedulera.
static void BottleNames_Load(void)
{
    const BottleNamesRecord *last = NULL;
    uint32_t slot = 0;

    for (; slot < NAMES_SLOT_COUNT; slot++)
    {
        const BottleNamesRecord *rec = NamesSlot(slot);
        if (rec->magic == NAMES_MAGIC)
        {
            last = rec;
        }
        else if (IsNamesSlotErased(slot))
        {
            break;
        }
    }
    namesNextSlot = slot;

    if (last != NULL)
    {
        for (uint8_t i = 0; i < BOTTLE_COUNT; i++)
        {
            memcpy(bottleName[i], last->names[i], BOTTLE_NAME_MAX);
            bottleName[i][BOTTLE_NAME_MAX] = '\0';
        }
        bottleNamesVersion++;
    }
}

static HAL_StatusTypeDef BottleNames_EraseSector(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sectorError = 0;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = NAMES_FLASH_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sectorError);
    // Flash jest czytany przez D-Cache - bez tego zostalyby stare dane.
    SCB_InvalidateDCache_by_Addr((uint32_t *)NAMES_FLASH_ADDR, NAMES_FLASH_SIZE);
    namesNextSlot = 0;
    return status;
}

// Zapisuje aktualne nazwy jako nowy rekord. Tylko z taska (nie z przerwania):
// kasowanie sektora zatrzymuje procesor na 1-2 s.
static void BottleNames_Save(void)
{
    BottleNamesRecord rec;
    memset(&rec, 0, sizeof(rec));

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    memcpy(rec.names, bottleName, sizeof(rec.names));
    bottleNamesDirty = 0;
    __set_PRIMASK(primask);
    rec.magic = NAMES_MAGIC;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_ALL_ERRORS);

    if (namesNextSlot >= NAMES_SLOT_COUNT || !IsNamesSlotErased(namesNextSlot))
    {
        if (BottleNames_EraseSector() != HAL_OK)
        {
            HAL_FLASH_Lock();
            return;
        }
    }

    uint32_t addr = (uint32_t)NamesSlot(namesNextSlot);
    const uint32_t *word = (const uint32_t *)&rec;
    for (uint32_t i = 0; i < sizeof(rec) / 4; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i * 4, word[i]) != HAL_OK)
        {
            break;
        }
    }
    SCB_InvalidateDCache_by_Addr((uint32_t *)(addr & ~31u), sizeof(rec) + 32);
    HAL_FLASH_Lock();

    namesNextSlot++;
}

uint8_t BLE_GetCommand(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint8_t cmd = ble_command;
    ble_command = 0;
    __set_PRIMASK(primask);
    return cmd;
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  // Daj czas HX711 na ustabilizowanie po wlaczeniu zasilania.
  osDelay(500);

  // TARA: zapamietujemy odczyt przy PUSTEJ belce jako punkt zerowy.
  // Wazne: przy starcie nic nie moze lezec na czujniku!
  hx711_offset = HX711_ReadAverage(20);

  /* Infinite loop */
  for(;;)
  {
    waga_surowa = HX711_ReadAverage(10);
    // Przeliczenie surowej wartosci na gramy wzgledem tary.
    waga_gramy = (int32_t)((waga_surowa - hx711_offset) / HX711_SCALE);

    // Aplikacja wysyla zwykle kilka $SET pod rzad - zapisujemy raz, po chwili ciszy.
    if (bottleNamesDirty && (HAL_GetTick() - bottleNamesChangedTick) >= NAMES_SAVE_DELAY_MS)
    {
        BottleNames_Save();
    }
    osDelay(100);
  }
  /* USER CODE END 5 */
}

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
