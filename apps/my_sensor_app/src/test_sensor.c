/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <console/console.h>

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
ADC_HandleTypeDef hadc1;

/* Private function prototypes -----------------------------------------------*/
static void MX_ADC1_Init(void);

/* 
 * STM32F103x data-sheet:
 * 5.3.19 Temperature sensor characteristics 
 * Table 50. TS characteristics, Page 80
 */
const float         AVG_SLOPE   = 4.3E-03;      // slope (gradient) of temperature line function  [V/°C]
const float         V25         = 1.43;         // sensor's voltage at 25°C [V]
const float         ADC_TO_VOLT = 3.3 / 4096;   // conversion coefficient of digital value to voltage [V] 
                                                // when using 3.3V ref. voltage at 12-bit resolution (2^12 = 4096)

int test_sensor(void) {

  /* Initialize all configured peripherals */
  MX_ADC1_Init();
  while(HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK);           // calibrate AD convertor

  while (1) {
    // uint16_t rawValue;
    float temp;

    HAL_ADC_Start(&hadc1);
    //  HAL_ADC_PollForConversion(&hadc1, 10 * 1000 /* HAL_MAX_DELAY */);
    while(HAL_ADC_PollForConversion(&hadc1, 1000000) != HAL_OK);  // wait for completing the conversion
    uint16_t adcValue = HAL_ADC_GetValue(&hadc1);                        // read sensor's digital value
    HAL_ADC_Stop(&hadc1);

    float vSense = adcValue * ADC_TO_VOLT;                            // convert sensor's digital value to voltage [V]
    /*
        * STM32F103xx Reference Manual:
        * 11.10 Temperature sensor
        * Reading the temperature, Page 235
        * Temperature (in °C) = {(V25 - Vsense) / Avg_Slope} + 25
        */
    temp = (V25 - vSense) / AVG_SLOPE + 25.0f;                  // convert sensor's output voltage to temperature [°C]
    
#ifdef NOTUSED
    temp = __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(AVG_SLOPE,
                                             V25,
                                             __TEMPSENSOR_CALX_TEMP__,
                                             __VREFANALOG_VOLTAGE__,
                                             adcValue,
                                             LL_ADC_RESOLUTION_12B)   

    rawValue = HAL_ADC_GetValue(&hadc1);
    temp = ((float)rawValue) / 4095 * 3300;
    temp = ((temp - 760.0) / 2.5) + 25;
#endif

    console_printf("rawtemp: %d\n", (int) adcValue);  ////
    console_printf("temp: ");  console_printfloat(temp);  console_printf("\n");  ////
    console_flush();
  }
}

/* ADC1 init function */
void MX_ADC1_Init(void) {
  ADC_ChannelConfTypeDef sConfig;

  /* Enable ADC peripheral */
  __HAL_RCC_ADC1_CLK_ENABLE();

  /**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
   */
  hadc1.Instance = ADC1;
  //// hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  //// hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;  ////  ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  //// hadc1.Init.DMAContinuousRequests = DISABLE;
  //// hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;  ////
  HAL_ADC_Init(&hadc1);

  /**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
   */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;  ////  ADC_SAMPLETIME_480CYCLES;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

#ifdef NOTUSED
    PLL = 64 MHz
    APB2 = PLL DIV 4 = 16 mhz    
    ADC input clock must not exceed 14 MHz

{
  uint32_t DataAlign;             
  /*!< Specifies ADC data alignment to right (MSB on register bit 11 and LSB on register bit 0) (default setting)
  or to left (if regular group: MSB on register bit 15 and LSB on register bit 4, if injected group (MSB kept as signed value due to potential negative value after offset application): MSB on register bit 14 and LSB on register bit 3).
  This parameter can be a value of @ref ADC_Data_align */

  uint32_t ScanConvMode;          
  /*!< Configures the sequencer of regular and injected groups.
  This parameter can be associated to parameter 'DiscontinuousConvMode' to have main sequence subdivided in successive parts.
  If disabled: Conversion is performed in single mode (one channel converted, the one defined in rank 1).
              Parameters 'NbrOfConversion' and 'InjectedNbrOfConversion' are discarded (equivalent to set to 1).
  If enabled:  Conversions are performed in sequence mode (multiple ranks defined by 'NbrOfConversion'/'InjectedNbrOfConversion' and each channel rank).
              Scan direction is upward: from rank1 to rank 'n'.
  This parameter can be a value of @ref ADC_Scan_mode
  Note: For regular group, this parameter should be enabled in conversion either by polling (HAL_ADC_Start with Discontinuous mode and NbrOfDiscConversion=1)
        or by DMA (HAL_ADC_Start_DMA), but not by interruption (HAL_ADC_Start_IT): in scan mode, interruption is triggered only on the
        the last conversion of the sequence. All previous conversions would be overwritten by the last one.
        Injected group used with scan mode has not this constraint: each rank has its own result register, no data is overwritten. */

  uint32_t ContinuousConvMode;    
  /*!< Specifies whether the conversion is performed in single mode (one conversion) or continuous mode for regular group,
  after the selected trigger occurred (software start or external trigger).
  This parameter can be set to ENABLE or DISABLE. */

  uint32_t NbrOfConversion;       
  /*!< Specifies the number of ranks that will be converted within the regular group sequencer.
  To use regular group sequencer and convert several ranks, parameter 'ScanConvMode' must be enabled.
  This parameter must be a number between Min_Data = 1 and Max_Data = 16. */

  uint32_t DiscontinuousConvMode; 
  /*!< Specifies whether the conversions sequence of regular group is performed in Complete-sequence/Discontinuous-sequence (main sequence subdivided in successive parts).
  Discontinuous mode is used only if sequencer is enabled (parameter 'ScanConvMode'). If sequencer is disabled, this parameter is discarded.
  Discontinuous mode can be enabled only if continuous mode is disabled. If continuous mode is enabled, this parameter setting is discarded.
  This parameter can be set to ENABLE or DISABLE. */

  uint32_t NbrOfDiscConversion;   
  /*!< Specifies the number of discontinuous conversions in which the  main sequence of regular group (parameter NbrOfConversion) will be subdivided.
  If parameter 'DiscontinuousConvMode' is disabled, this parameter is discarded.
  This parameter must be a number between Min_Data = 1 and Max_Data = 8. */

  uint32_t ExternalTrigConv;      
  /*!< Selects the external event used to trigger the conversion start of regular group.
  If set to ADC_SOFTWARE_START, external triggers are disabled.
  If set to external trigger source, triggering is on event rising edge.
  This parameter can be a value of @ref ADC_External_trigger_source_Regular */

}ADC_InitTypeDef;

  uint32_t Channel;                
  /*!< Specifies the channel to configure into ADC regular group.
  This parameter can be a value of @ref ADC_channels
  Note: Depending on devices, some channels may not be available on package pins. Refer to device datasheet for channels availability.
  Note: On STM32F1 devices with several ADC: Only ADC1 can access internal measurement channels (VrefInt/TempSensor) 
  Note: On STM32F10xx8 and STM32F10xxB devices: A low-amplitude voltage glitch may be generated (on ADC input 0) on the PA0 pin, when the ADC is converting with injection trigger.
        It is advised to distribute the analog channels so that Channel 0 is configured as an injected channel.
        Refer to errata sheet of these devices for more details. */

  uint32_t Rank;                   
  /*!< Specifies the rank in the regular group sequencer 
  This parameter can be a value of @ref ADC_regular_rank
  Note: In case of need to disable a channel or change order of conversion sequencer, rank containing a previous channel setting can be overwritten by the new channel setting (or parameter number of conversions can be adjusted) */

  uint32_t SamplingTime;           
  /*!< Sampling time value to be set for the selected channel.
  Unit: ADC clock cycles
  Conversion time is the addition of sampling time and processing time (12.5 ADC clock cycles at ADC resolution 12 bits).
  This parameter can be a value of @ref ADC_sampling_times
  Caution: This parameter updates the parameter property of the channel, that can be used into regular and/or injected groups.
            If this same channel has been previously configured in the other group (regular/injected), it will be updated to last setting.
  Note: In case of usage of internal measurement channels (VrefInt/TempSensor),
        sampling time constraints must be respected (sampling time can be adjusted in function of ADC clock frequency and sampling time setting)
        Refer to device datasheet for timings values, parameters TS_vrefint, TS_temp (values rough order: 5us to 17.1us min). */

}ADC_ChannelConfTypeDef;

    /* Configure PLL ------------------------------------------------------*/
    /* PLL configuration: PLLCLK = (HSI / 2) * PLLMUL = (8 / 2) * 16 = 64 MHz */
    /* PREDIV1 configuration: PREDIV1CLK = PLLCLK / HSEPredivValue = 64 / 1 = 64 MHz */
    /* Enable HSI and activate PLL with HSi_DIV2 as source */
    oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscinitstruct.HSEState = RCC_HSE_OFF;
    oscinitstruct.LSEState = RCC_LSE_OFF;
    oscinitstruct.HSIState = RCC_HSI_ON;
    oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscinitstruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscinitstruct.PLL.PLLState = RCC_PLL_ON;
    oscinitstruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    oscinitstruct.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&oscinitstruct) != HAL_OK) {
        assert(0);
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
       clocks dividers */
    clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                               RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV4;
    clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK) {
        assert(0);
    }
#endif

/**
  * @brief  Helper macro to calculate the temperature (unit: degree Celsius)
  *         from ADC conversion data of internal temperature sensor.
  * @note   Computation is using temperature sensor typical values
  *         (refer to device datasheet).
  * @note   Calculation formula:
  *           Temperature = (TS_TYP_CALx_VOLT(uV) - TS_ADC_DATA * Conversion_uV)
  *                         / Avg_Slope + CALx_TEMP
  *           with TS_ADC_DATA      = temperature sensor raw data measured by ADC
  *                                   (unit: digital value)
  *                Avg_Slope        = temperature sensor slope
  *                                   (unit: uV/Degree Celsius)
  *                TS_TYP_CALx_VOLT = temperature sensor digital value at
  *                                   temperature CALx_TEMP (unit: mV)
  *         Caution: Calculation relevancy under reserve the temperature sensor
  *                  of the current device has characteristics in line with
  *                  datasheet typical values.
  *                  If temperature sensor calibration values are available on
  *                  on this device (presence of macro __LL_ADC_CALC_TEMPERATURE()),
  *                  temperature calculation will be more accurate using
  *                  helper macro @ref __LL_ADC_CALC_TEMPERATURE().
  * @note   As calculation input, the analog reference voltage (Vref+) must be
  *         defined as it impacts the ADC LSB equivalent voltage.
  * @note   Analog reference voltage (Vref+) must be known from
  *         user board environment or can be calculated using ADC measurement.
  * @note   ADC measurement data must correspond to a resolution of 12bits
  *         (full scale digital value 4095). If not the case, the data must be
  *         preliminarily rescaled to an equivalent resolution of 12 bits.
  * @param  __TEMPSENSOR_TYP_AVGSLOPE__   Device datasheet data: Temperature sensor slope typical value (unit: uV/DegCelsius).
  *                                       On STM32F1, refer to device datasheet parameter "Avg_Slope".
  * @param  __TEMPSENSOR_TYP_CALX_V__     Device datasheet data: Temperature sensor voltage typical value (at temperature and Vref+ defined in parameters below) (unit: mV).
  *                                       On STM32F1, refer to device datasheet parameter "V25".
  * @param  __TEMPSENSOR_CALX_TEMP__      Device datasheet data: Temperature at which temperature sensor voltage (see parameter above) is corresponding (unit: mV)
  * @param  __VREFANALOG_VOLTAGE__        Analog voltage reference (Vref+) voltage (unit: mV)
  * @param  __TEMPSENSOR_ADC_DATA__       ADC conversion data of internal temperature sensor (unit: digital value).
  * @param  __ADC_RESOLUTION__            ADC resolution at which internal temperature sensor voltage has been measured.
  *         This parameter can be one of the following values:
  *         @arg @ref LL_ADC_RESOLUTION_12B
  * @retval Temperature (unit: degree Celsius)
  */
/* 
#define __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(__TEMPSENSOR_TYP_AVGSLOPE__,
                                             __TEMPSENSOR_TYP_CALX_V__,
                                             __TEMPSENSOR_CALX_TEMP__,
                                             __VREFANALOG_VOLTAGE__,
                                             __TEMPSENSOR_ADC_DATA__,
                                             __ADC_RESOLUTION__)   
*/
