/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  * I2C OLED zaslon (128x64, kontroler SH1106) na STM32F446RE (Nucleo-F446RE)
  *
  * Program najprej s skeniranjem I2C poišče naprave na vodilu (rezultat se
  * izpiše prek UART), nato inicializira zaslon in na njem prikaže napis
  * "Hello OLED" ter števec sekund od zagona.
  *
  * Ožičenje zaslona:
  *   GND -> GND,  VCC -> 3V3,  SCL -> PB8 (D15),  SDA -> PB9 (D14)
  *
  * Delovanje: v pomnilniku (oled_buf) se sestavi slika, funkcija oled_update()
  * jo nato pošlje zaslonu po I2C. Števec se osvežuje brez blokirajočega
  * čakanja (HAL_GetTick), vsako sekundo.
  *
  * UART (USART2, 9600 baud) se uporablja samo za izpis rezultata skeniranja.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>		// snprintf: sestavljanje besedila za izpis
#include <string.h>		// memset, strlen
#include "font5x7.h"	// pisava 5x7 pikslov (ASCII 32 do 126)
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define OLED_ADDR (0x3C << 1)	// I2C naslov zaslona (7-bitni 0x3C), HAL ga potrebuje pomaknjenega za 1 bit

uint8_t oled_buf[1024];		// framebuffer: slika zaslona v RAM-u (128 stolpcev x 8 strani po 8 pikslov)
uint32_t oled_start = 0;	// trenutek (HAL_GetTick) zadnjega osveževanja zaslona
char oled_txt[22];			// besedilo za izpis (največ 21 znakov v vrstici)

/**
  * @brief  Pošlje en ukaz zaslonu.
  * @param  cmd: ukazni bajt kontrolerja
  * @note   Kontrolni bajt 0x00 pomeni, da je naslednji bajt ukaz (0x40 = podatki za sliko).
  */
void oled_cmd(uint8_t cmd){
    HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

/**
  * @brief  Pošlje framebuffer zaslonu SH1106, stran po stran.
  * @note   SH1106 ne podpira vodoravnega naslavljanja, zato se vsaka od 8 strani
  *         (po 128 bajtov) pošlje posebej. SH1106 ima 132 stolpcev v pomnilniku,
  *         prikazanih je 128, zato je začetni stolpec 2.
  */
void oled_update(void){
    for(uint8_t page = 0; page < 8; page++){
        oled_cmd(0xB0 + page);		// izberi stran 0 do 7
        oled_cmd(0x02);				// začetni stolpec, nižji del (zamik 2)
        oled_cmd(0x10);				// začetni stolpec, višji del
        HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT,
                          &oled_buf[page * 128], 128, 100);		// 0x40 = sledijo podatki za sliko
    }
}

/**
  * @brief  Prižge ali ugasne en piksel v framebufferju.
  * @param  x: stolpec (0 do 127), y: vrstica (0 do 63), on: 1 = prižgan, 0 = ugasnjen
  * @note   Zaslonu piksla ne pošlje, za to je potreben oled_update().
  */
void oled_pixel(uint8_t x, uint8_t y, uint8_t on){
    if(x > 127 || y > 63){
        return;		// zunaj zaslona: ne pišemo v pomnilnik
    }

    uint16_t indeks = x + (y / 8) * 128;	// kateri bajt v bufferju
    uint8_t bit = y % 8;					// kateri bit v tem bajtu

    if(on){
        oled_buf[indeks] |= (1 << bit);		// prižgi piksel
    }
    else{
        oled_buf[indeks] &= ~(1 << bit);	// ugasni piksel
    }
}

/**
  * @brief  Izriše en znak (5x7 pikslov) v framebuffer.
  * @param  x, y: položaj zgornjega levega kota znaka, c: znak (ASCII 32 do 126)
  */
void oled_putc(uint8_t x, uint8_t y, char c){
    if(c < 32 || c > 126){
        c = '?';		// znak izven tabele
    }
    for(uint8_t col = 0; col < 5; col++){
        uint8_t stolpec = font5x7[c - 32][col];		// bajt = en stolpec znaka, najnižji bit je vrh
        for(uint8_t row = 0; row < 8; row++){
            oled_pixel(x + col, y + row, (stolpec >> row) & 1);
        }
    }
}

/**
  * @brief  Izpiše besedilo v framebuffer. Vsak znak zavzame 6 pikslov (5 + 1 presledek).
  * @param  x, y: položaj prvega znaka, s: besedilo (zaključeno z '\0')
  */
void oled_print(uint8_t x, uint8_t y, const char *s){
    while(*s){
        if(x > 122){
            break;		// ni več prostora v vrstici
        }
        oled_putc(x, y, *s);
        x += 6;
        s++;
    }
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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  char msg[40];		// vmesno besedilo za izpis prek UART

  /* 1. I2C skeniranje: preveri vse naslove 1 do 127 in izpiše tiste, ki odgovorijo.
   *    Zaslon se mora javiti kot 0x3C. */
  HAL_UART_Transmit(&huart2, (uint8_t *)"I2C skeniranje...\r\n", 19, 100);

  for(uint8_t addr = 1; addr < 128; addr++){
      /* HAL potrebuje 7-bitni naslov, pomaknjen za 1 bit v levo.
       * 1 = število poskusov, 10 = timeout v ms. */
      if(HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK){
          snprintf(msg, sizeof(msg), "Najden: 0x%02X\r\n", addr);
          HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
      }
  }

  HAL_UART_Transmit(&huart2, (uint8_t *)"Konec\r\n", 7, 100);

  /* 2. Inicializacija zaslona. Brez nje zaslon ne prikaže slike.
   *    Uporabljeno je običajno zaporedje za SSD1306, ki ga SH1106 sprejme. */
  uint8_t init[] = {
      0xAE,			// zaslon ugasnjen
      0xD5, 0x80,		// frekvenca ure zaslona
      0xA8, 0x3F,		// višina: 64 vrstic
      0xD3, 0x00,		// brez zamika slike
      0x40,			// začetna vrstica 0
      0x8D, 0x14,		// vklop notranje črpalke napetosti (nujno za OLED)
      0x20, 0x00,		// vodoravno naslavljanje (SH1106 ga ne uporablja, slike pošiljamo po straneh)
      0xA1, 0xC8,		// obračanje slike (levo/desno, zgoraj/spodaj)
      0xDA, 0x12,		// razporeditev pinov kontroler-zaslon
      0x81, 0xCF,		// kontrast
      0xD9, 0xF1,		// čas predpolnitve
      0xDB, 0x40,		// nastavitev napetosti VCOMH
      0xA4,			// prikaži vsebino pomnilnika
      0xA6,			// normalna (ne obrnjena) slika
      0xAF			// zaslon prižgan
  };

  for(uint8_t k = 0; k < sizeof(init); k++){
      oled_cmd(init[k]);
  }

  /* 3. Prvi izpis: najprej počisti buffer, nato nariši besedilo in ga pošlji zaslonu.
   *    To mora biti po inicializaciji, sicer se slika izgubi. */
  memset(oled_buf, 0, sizeof(oled_buf));
  oled_print(0, 0, "Hello OLED");
  oled_update();
  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED2);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Števec: slika se osveži vsako sekundo.
     * Čas se meri z HAL_GetTick(), zato program med čakanjem ne stoji. */
    if(HAL_GetTick() - oled_start >= 1000){
        oled_start = HAL_GetTick();
        snprintf(oled_txt, sizeof(oled_txt), "Cas: %lu s", HAL_GetTick() / 1000);
        memset(oled_buf, 0, sizeof(oled_buf));		// počisti staro sliko
        oled_print(0, 0, "Hello OLED");
        oled_print(0, 16, oled_txt);				// druga vrstica (y = 16)
        oled_update();								// pošlji sliko zaslonu
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

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
