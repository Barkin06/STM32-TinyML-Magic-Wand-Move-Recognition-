/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.cpp
  * @brief          : Main program body (LIS3DSH + Edge Impulse TinyML)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Edge Impulse Kütüphaneleri */
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

/* Edge Impulse Porting Functions */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "usbd_cdc_if.h" // USB Haberleşmesi
#include <string.h>      // strlen için

// Edge Impulse'ın C++ fonksiyonları (extern "C" sarmalayıcısı olmadan)
void *ei_malloc(size_t size) { return malloc(size); }
void *ei_calloc(size_t nitems, size_t size) { return calloc(nitems, size); }
void ei_free(void *ptr) { free(ptr); }

// Yapay zeka loglarını doğrudan USB üzerinden Terminale (Tera Term) gönderir
void ei_printf(const char *format, ...) {
    char print_buffer[128];
    va_list myargs;
    va_start(myargs, format);
    vsnprintf(print_buffer, sizeof(print_buffer), format, myargs);
    va_end(myargs);
    CDC_Transmit_FS((uint8_t*)print_buffer, strlen(print_buffer));
    HAL_Delay(5); // USB'nin veriyi yollaması için küçük bir nefes payı
}

void ei_printf_float(float f) {
    ei_printf("%f", f);
}

uint64_t ei_read_timer_ms() { return HAL_GetTick(); }
uint64_t ei_read_timer_us() { return (uint64_t)HAL_GetTick() * 1000; }

EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// --- LIS3DSH Register Tanımları ---
#define WHO_AM_I_ADDR     0x0F
#define CTRL_REG4_ADDR    0x20
#define CTRL_REG5_ADDR    0x24
#define OUT_X_L_ADDR      0x28
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

I2S_HandleTypeDef hi2s3;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
char msg_buffer[64];
extern SPI_HandleTypeDef hspi1;

/* Inference (Yapay Zeka) Değişkenleri */
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t feature_ix = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// --- LIS3DSH YAZMA FONKSİYONU ---
void LIS3DSH_Write(uint8_t reg, uint8_t data) {
    uint8_t txData[2];
    txData[0] = reg;       // Yazma işlemi (MSB 0)
    txData[1] = data;

    // CS Pinini Low yap (Seç) -> PE3
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);
    // Veriyi gönder
    HAL_SPI_Transmit(&hspi1, txData, 2, 100);
    // CS Pinini High yap (Bırak) -> PE3
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
}

// --- LIS3DSH OKUMA FONKSİYONU ---
void LIS3DSH_Read(uint8_t reg, uint8_t *data, uint8_t count) {
    // Okuma işlemi için MSB bitini 1 yapmalıyız (0x80 | reg)
    uint8_t addr = reg | 0x80;

    // CS Low (Seç)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);

    // 1. Adresi Gönder
    HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
    // 2. Veriyi Al
    HAL_SPI_Receive(&hspi1, data, count, 100);

    // CS High (Bırak)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
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
  MX_I2C1_Init();
  MX_I2S3_Init();
  MX_SPI1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(2000); // Sistem ve USB'nin oturması için bekle

  // Sensörün CS pini konuşma yokken HIGH olmalıdır.
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
  HAL_Delay(100);

  uint8_t ctrl_val = 0;
  uint8_t chip_id = 0;
  int retry_count = 0;

  // 1. ID KONTROLÜ
  LIS3DSH_Read(WHO_AM_I_ADDR, &chip_id, 1);

  // 2. AYARLARI ZORLA KABUL ETTİRME DÖNGÜSÜ
  while(ctrl_val != 0x67 && retry_count < 10) {
      // Ayar Gönder: 100Hz, XYZ Aktif
      LIS3DSH_Write(CTRL_REG4_ADDR, 0x67);
      HAL_Delay(10);

      // Ayar Gönder: +/- 8g (Kilitlenmeyi önler)
      LIS3DSH_Write(CTRL_REG5_ADDR, 0x18);
      HAL_Delay(10);

      // KONTROL ET: Yazdığımızı okuyalım
      LIS3DSH_Read(CTRL_REG4_ADDR, &ctrl_val, 1);

      if(ctrl_val != 0x67) {
          retry_count++;
          HAL_Delay(50);
      }
  }

  // DURUM RAPORU
  if (ctrl_val == 0x67) {
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET); // Mavi LED
      sprintf(msg_buffer, "Sensor Hazir (ID:%02X). TinyML Basliyor...\r\n", chip_id);
  } else {
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET); // Kırmızı LED
      sprintf(msg_buffer, "HATA: Sensor Ayari Almadi! (Okunan: %02X)\r\n", ctrl_val);
  }

  CDC_Transmit_FS((uint8_t*)msg_buffer, strlen(msg_buffer));
  HAL_Delay(2000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

    while (1)
    {
      /* USER CODE END WHILE */

      /* USER CODE BEGIN 3 */
      uint8_t xl, xh, yl, yh, zl, zh;
      int16_t x_val, y_val, z_val;

      if (ctrl_val == 0x67) {
          // 1. SENSÖRDEN VERİ OKU
          LIS3DSH_Read(0x28, &xl, 1);
          LIS3DSH_Read(0x29, &xh, 1);
          x_val = (int16_t)((xh << 8) | xl);

          LIS3DSH_Read(0x2A, &yl, 1);
          LIS3DSH_Read(0x2B, &yh, 1);
          y_val = (int16_t)((yh << 8) | yl);

          LIS3DSH_Read(0x2C, &zl, 1);
          LIS3DSH_Read(0x2D, &zh, 1);
          z_val = (int16_t)((zh << 8) | zl);

          // 2. ÇARPAN YOK: Grafikteki gibi Ham (Raw) veriyi ekliyoruz
          features[feature_ix++] = (float)x_val;
          features[feature_ix++] = (float)y_val;
          features[feature_ix++] = (float)z_val;

          // 3. BUFFER DOLDUYSA TAHMİN (INFERENCE) YAP
		  if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {

			  ei_printf("\n[TEST] Son Okunan Z: %d\r\n", z_val); // Sadece Z'yi görsek yeter

			  signal_t signal;
			  int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

			  if (err == 0) {
				  ei_impulse_result_t result = { 0 };
				  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

				  if (res == EI_IMPULSE_OK) {
					  float en_yuksek_skor = 0.0f;
					  const char* algilanan_hareket = "Bilinmiyor";

					  // En yüksek skoru bul
					  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
						  if (result.classification[ix].value > en_yuksek_skor) {
							  en_yuksek_skor = result.classification[ix].value;
							  algilanan_hareket = result.classification[ix].label;
						  }
					  }

					  int tam = (int)en_yuksek_skor;
					  int ondalik = (int)((en_yuksek_skor - tam) * 100);

					  // SÜSSÜZ VE TERTEMİZ ÇIKTI:
					  ei_printf("-> Tahmin: %s (%d.%02d)\r\n", algilanan_hareket, tam, ondalik);
				  }
			  }

			  // Buffer'ı sıfırla, yeni hareketi dinlemeye baştan başla
			  feature_ix = 0;

          }

          HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15); // Mavi LED kalp atışı
      } else {
          HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14); // Hata LED'i
      }

      // İŞTE BÜTÜN DENGEYİ SAĞLAYAN SATIR: Tekrar 100Hz (10ms) gecikmeye döndük!
      HAL_Delay(10);
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
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_96K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MEMS_CS_GPIO_Port, MEMS_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : MEMS_CS_Pin */
  GPIO_InitStruct.Pin = MEMS_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MEMS_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT1_Pin */
  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CLK_IN_Pin */
  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
  * where the assert_param error has occurred.
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
