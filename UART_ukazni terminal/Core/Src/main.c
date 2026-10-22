/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  * UART ukazni terminal za STM32F446RE (Nucleo-F446RE)
  *
  * Ukaze pošiljaš prek serijskega terminala (USART2, 9600 baud, 8N1).
  * Vsak ukaz se zaključi s tipko Enter (znak '\r'). Ukazi ločijo velike in male črke.
  *
  *   led on        prižge LED LD2
  *   led off       ugasne LED LD2
  *   led blink N   LED LD2 utripa, N = interval preklopa v ms (veljavno 50 do 5000)
  *   pwm N         svetilnost zunanje LED na PA8, N = odstotek (veljavno 0 do 100)
  *
  * Plošča po vsakem ukazu odgovori "OK" (veljaven ukaz) ali "Error" (neveljaven).
  *
  * Delovanje: znake sprejema prekinitev (callback), ukaz obdela glavna zanka.
  * Program nikjer ne čaka (ne uporablja HAL_Delay), zato plošča med utripanjem
  * še vedno sprejema ukaze.
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
#include <string.h>		// strcmp: primerjava celotnega besedila
#include <stdio.h>		// sscanf: branje števila iz ukaza
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
TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * Spremenljivke, ki si jih delita callback (prekinitev) in glavna zanka.
 * Označene so z "volatile", ker se lahko vrednost spremeni kadarkoli v prekinitvi.
 */
volatile uint8_t data[20];				// buffer za sprejeti ukaz (največ 19 znakov + '\0')
uint16_t Size = 20;						// se ne uporablja (velikost bufferja se računa s sizeof(data))
volatile uint8_t rx;					// en pravkar prejet znak
volatile uint8_t ukaz_pripravljen = 0;	// zastavica: 1 = prejet cel ukaz (Enter), 0 = ukaz še ni končan
volatile uint8_t i = 0;					// mesto v bufferju data, kamor pride naslednji znak

/* Spremenljivke za utripanje LED (uporablja jih samo glavna zanka) */
uint8_t utripa = 0;						// 0 = LED ne utripa, 1 = LED utripa
uint32_t interval = 0;					// čas med dvema preklopoma LED v ms
uint32_t utrip_start = 0;				// trenutek (HAL_GetTick) zadnjega preklopa LED
uint32_t n = 0;							// vmesna vrednost, prebrana iz ukaza (blink ali pwm);
										// najprej preverimo, šele nato uporabimo

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
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);	// zažene PWM na PA8 (začetna svetilnost 0 %)

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  HAL_NVIC_EnableIRQ(USART2_IRQn);	// vklopi prekinitev USART2 (sprejem znakov)

  /* Zažene sprejem 1 znaka v spremenljivko rx. Ko znak prispe, HAL pokliče
   * HAL_UART_RxCpltCallback (spodaj), ki mora sprejem ponovno zagnati. */
  HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx, 1);

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	/* 1. DEL: obdelava ukaza.
	 * Izvede se samo, ko je callback sprejel cel ukaz (po pritisku na Enter). */
	if(ukaz_pripravljen){

		if(strcmp((char *)data, "led on") == 0){
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);	// prižgi LED
			utripa = 0;			// ustavi utripanje, sicer bi prepisalo ta ukaz
			HAL_UART_Transmit(&huart2, (uint8_t *)"OK\r\n", 4, 100);
		}
		else if(strcmp((char *)data, "led off") == 0){
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);	// ugasni LED
			utripa = 0;			// ustavi utripanje
			HAL_UART_Transmit(&huart2, (uint8_t *)"OK\r\n", 4, 100);
		}
		/* Ukaz s številko: sscanf primerja besedilo z vzorcem "led blink <število>".
		 * Vrne 1, če se besedilo ujema in je število prebrano. Interval je veljaven
		 * med 50 in 5000 ms. */
		else if(sscanf((char *)data, "led blink %lu", &n) == 1 && n >= 50 && n <= 5000){
			interval = n;					// shrani nov interval
			utrip_start = HAL_GetTick();	// štetje časa se začne zdaj
			utripa = 1;						// vklopi utripanje (izvaja ga 2. del spodaj)
			HAL_UART_Transmit(&huart2, (uint8_t *)"OK\r\n", 4, 100);
		}
		/* Ukaz "pwm N": svetilnost zunanje LED na PA8 v odstotkih (0 do 100).
		 * ARR je 999, zato je CCR = N * 10 (npr. 50 % -> 500). */
		else if(sscanf((char *)data, "pwm %lu", &n) == 1 && n <= 100){
			__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, n * 10);
			HAL_UART_Transmit(&huart2, (uint8_t *)"OK\r\n", 4, 100);
		}
		else{
			/* neznan ukaz, napačna oblika ali vrednost izven obsega */
			HAL_UART_Transmit(&huart2, (uint8_t *)"Error\r\n", 7, 100);
		}

		/* Priprava na naslednji ukaz (tudi po napaki, da se buffer vedno počisti) */
		i = 0;
		ukaz_pripravljen = 0;
	}

	/* 2. DEL: utripanje LED LD2.
	 * Izvaja se v vsakem obhodu zanke, neodvisno od ukazov. Čas se meri z
	 * HAL_GetTick(), zato program ne čaka in lahko med utripanjem sprejema ukaze. */
	if(utripa && HAL_GetTick() - utrip_start >= interval){
		utrip_start = HAL_GetTick();
		HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);		// obrni stanje LED
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 83;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Callback, ki ga HAL pokliče, ko je sprejet znak (prekinitev USART2).
  * @note   Callback naj bo kratek: znak samo shrani v buffer. Ukaz obdela
  *         glavna zanka, ko vidi, da je ukaz_pripravljen = 1.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){

	/* Dokler glavna zanka ukaza ni obdelala, novih znakov ne shranjujemo,
	 * da se buffer ne prepiše med primerjavo. */
	if(!ukaz_pripravljen){
		if(rx == '\r'){							// Enter = konec ukaza
			data[i] = '\0';						// zaključi besedilo
			ukaz_pripravljen = 1;				// obvesti glavno zanko
		}
		/* '\n' ignoriramo (nekateri terminali pošljejo \r\n). Znake shranimo samo,
		 * dokler je v bufferju prostor (ena pozicija je rezervirana za '\0'),
		 * s tem preprečimo prepis pomnilnika pri predolgi vrstici. */
		else if(rx != '\n' && i < sizeof(data) - 1){
			data[i] = rx;						// shrani znak
			i++;								// premakni se na naslednje mesto
		}
	}

	/* HAL sprejme samo toliko znakov, kot je zahtevano, zato je treba
	 * sprejem po vsakem znaku ponovno zagnati. */
	HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx, 1);
}

/**
  * @brief  Callback ob napaki UART (npr. preobremenitev, napačen okvir).
  * @note   Sprejem ponovno zaženemo, da plošča po napaki ne preneha sprejemati.
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){

	HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx, 1);
}
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
