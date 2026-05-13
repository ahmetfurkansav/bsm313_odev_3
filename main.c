/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  AHMET FURKAN SAV
  23010310064
  * @file           : main.c
  * @brief          : Gömülü Sistemler Ödev 3 - FSM, Timer Kesmeleri ve Flash Yönetimi
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Sistem bloklanmasını önlemek amacıyla tasarlanan Sonlu Durum Makinesi (FSM) tanımları
typedef enum {
    STATE_BLINK_ON,
    STATE_BLINK_OFF,
    STATE_WAIT
} LedState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// STM32F103C8T6 için Sayfa 63 bellek adresi (1 KB sayfa boyutu)
#define FLASH_USER_ADDR 0x0800FC00 
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
uint32_t blink_count = 4;
uint32_t current_blink = 0;
uint32_t wait_seconds = 0;
LedState_t led_state = STATE_BLINK_ON;
uint8_t previous_button_state = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */
// Algoritmik NVM (Non-Volatile Memory) Yazma Fonksiyonu
void Flash_Write_Value(uint32_t data) {
    HAL_FLASH_Unlock(); // Bellek kilit mekanizmasını devre dışı bırak
    
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;
    
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = FLASH_USER_ADDR;
    EraseInitStruct.NbPages = 1;
    
    // İlgili bellek bloğunun silinmesi (Erase cycle)
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    
    // 32-bit (Word) uzunluğunda verinin programlanması
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_ADDR, data);
    
    HAL_FLASH_Lock(); // Güvenlik için belleği tekrar kilitle
}
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
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
  
  // ÖÇ h adımı: PA1 pini başlatılır ve lojik 0 düzeyinde sabitlenir.
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

  // NVM Okuma Algoritması: Flash bellekten doğrudan bellek erişimi ile değer okunur.
  // Cast işlemi: İlgili bellek adresi 32-bitlik işaretsiz tamsayı pointer'ına dönüştürülüp değeri alınır.
  uint32_t flash_val = *(__IO uint32_t *)FLASH_USER_ADDR;
  
  // Sınır Değer (Boundary) Kontrolü: 0xFFFF (yazılmamış) veya 4-7 dışı değerler filtrelenir.
  if (flash_val > 7 || flash_val < 4) { 
      blink_count = 4;
      Flash_Write_Value(blink_count);
  } else {
      blink_count = flash_val;
  }

  // ÖÇ g adımı: Soğuk Başlangıçta (Cold Boot) Fabrika Ayarlarına Dönüş Kontrolü
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) { 
      uint32_t boot_tick_start = HAL_GetTick();
      uint8_t reset_system = 1;
      
      // Kesintisiz 3000 ms basılı tutulma doğrulaması
      while ((HAL_GetTick() - boot_tick_start) < 3000) {
          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
              reset_system = 0; // İstenen süre dolmadan buton bırakıldı
              break;
          }
      }
      
      if (reset_system) {
          blink_count = 4; 
          Flash_Write_Value(blink_count);
          // Makinenin normal döngüye geçebilmesi için donanımsal kesintinin sonlanması beklenir
          while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET); 
      }
  }

  // Timer kesmelerini (interrupt) donanım seviyesinde başlat
  HAL_TIM_Base_Start_IT(&htim2);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // Yükselen Kenar Tetiklemeli Debouncing Algoritması
      uint8_t current_button_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
      
      // Durum geçişi tespiti (Lojik 0 -> Lojik 1)
      if (current_button_state == GPIO_PIN_SET && previous_button_state == GPIO_PIN_RESET) {
          HAL_Delay(50); // Mekanik titreşim sönümleme süresi
          
          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
              
              blink_count++;
              if (blink_count > 7) { 
                  blink_count = 4;
              }
              
              Flash_Write_Value(blink_count);
              
              // Tekli artırımın garanti altına alınması için sonsuz bekleme (Polling)
              while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);
          }
      }
      previous_button_state = current_button_state;
      
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
// Donanımsal Zamanlayıcı (Timer 2) Kesme Servis Rutini (ISR)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        
        // Sonlu Durum Makinesi (FSM) İşletimi
        switch(led_state) {
            case STATE_BLINK_ON:
                // PC13 Active-Low mantığıyla çalışır (Lojik 0'da LED yanar)
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); 
                led_state = STATE_BLINK_OFF;
                break;
                
            case STATE_BLINK_OFF:
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); 
                current_blink++;
                
                // Belirlenen blink (yanıp sönme) kotasına ulaşıldı mı kontrolü
                if (current_blink >= blink_count) {
                    current_blink = 0;
                    led_state = STATE_WAIT; // Asenkron bekleme durumuna geçiş
                    wait_seconds = 0;
                } else {
                    led_state = STATE_BLINK_ON; // Döngüye devam et
                }
                break;
                
            case STATE_WAIT:
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); 
                wait_seconds++;
                
                // 5 saniyelik ölü zaman (dead-time) kontrolü
                if (wait_seconds >= 5) { 
                    led_state = STATE_BLINK_ON; // Sistemin başlangıç durumuna resetlenmesi
                }
                break;
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief System Clock Configuration (CubeIDE tarafından doldurulur)
  */
void SystemClock_Config(void)
{
  /* Bu alan STM32CubeIDE arayüzünde yaptığınız 72MHz HCLK 
     ayarlarına göre otomatik olarak oluşturulacaktır. */
}

/**
  * @brief GPIO Initialization Function (CubeIDE tarafından doldurulur)
  */
static void MX_GPIO_Init(void)
{
  /* Bu alan CubeIDE tarafından otomatik doldurulacaktır. 
     İçerisinde PC13 Output, PA0 Input, PA1 Output ayarları yer alır. */
}

/**
  * @brief TIM2 Initialization Function (CubeIDE tarafından doldurulur)
  */
static void MX_TIM2_Init(void)
{
  /* Bu alan CubeIDE tarafından otomatik doldurulacaktır. 
     İçerisinde Prescaler: 7199 ve ARR: 9999 ayarları yer alır. */
}

/**
  * @brief  This function is executed in case of error occurrence.
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