/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "dma.h"
#include "fatfs.h"
#include "rtc.h"
#include "sdio.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "tim.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <cstdio>

#include "canvas.h"
#include "fs.h"
#include "st7735.h"
#include "unicode_render.h"
#include "pic_types.h"
#include "unicode_font_types.h"
#include "video_types.h"
#include "easy_menu.h"
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

/* USER CODE BEGIN PV */
UnicodeFont global_font;
Canvas global_canvas(160, 128);

easy_menu::Render render = {
    [](const char* str, uint16_t x, uint16_t y, bool color_inversion, void* data) {
        auto canvas = static_cast<Canvas*>(data);
        if (!color_inversion) canvas->WriteUnicodeString(x, y, str, &global_font, ST7735_WHITE, ST7735_BLACK);
        else canvas->WriteUnicodeString(x, y, str, &global_font, ST7735_BLACK, ST7735_WHITE);
    },
    [](uint16_t x, uint16_t y, uint16_t w, uint16_t h, void* data) {
        static_cast<Canvas*>(data)->FillRectangle(x, y, w, h, ST7735_WHITE);
    },
    [](uint16_t x, uint16_t y, uint16_t w, uint16_t h, void* data) {
        static_cast<Canvas*>(data)->FillRectangle(x, y, w, h, ST7735_BLACK);
    },
    [](const char* str) {
        uint16_t w = UnicodeStringUTF8Length(str, &global_font);
        uint16_t h = global_font.GetDefaultHeight();
        return std::pair{w, h};
    },
    [](uint16_t x, uint16_t y, void* data) {
        static_cast<Canvas*>(data)->DrawCanvasDMA(x, y);
    },
    [](uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t x0, uint16_t y0, void* data) {
        static_cast<Canvas*>(data)->Copy(x, y, w, h, x0, y0);
    },
    HAL_GetTick,
    &global_canvas,
};

volatile easy_menu::InputEvent input = {false, false, false, false, false};
volatile bool return_home;

class ButtonStateMachine;

using ButtonCallback = void(*)(const volatile ButtonStateMachine& button);

class ButtonStateMachine {
public:
    enum State {
        IDLE,
        DEBOUNCE_PRESS,
        PRESSED,
        DEBOUNCE_RELEASE
    };

    volatile State state = IDLE;
    volatile uint8_t debounce_count = 0;
    volatile bool *click_down, *click_up;
    GPIO_TypeDef* GPIO_PORT;
    uint16_t GPIO_PIN;
    ButtonCallback click_callback;

    static constexpr uint8_t DEBOUNCE_TICKS = 2;

    void onInterrupt(bool pin_state) volatile {
        switch (state) {
        case IDLE:
            if (!pin_state) {
                state = DEBOUNCE_PRESS;
                debounce_count = 0;
            }
            break;
        case PRESSED:
            if (pin_state) {
                state = DEBOUNCE_RELEASE;
                debounce_count = 0;
            }
            break;
        default:
            break;
        }
    }

    void onTimerTick() volatile {
        switch (state) {
        case DEBOUNCE_PRESS:
            debounce_count++;
            if (debounce_count >= DEBOUNCE_TICKS) {
                bool pin_state = HAL_GPIO_ReadPin(GPIO_PORT, GPIO_PIN) == GPIO_PIN_SET;
                if (!pin_state) {
                    if (click_down) *click_down = true;
                    if (click_callback) click_callback(*this);
                    state = PRESSED;
                }
                else {
                    state = IDLE;
                }
            }
            break;
        case DEBOUNCE_RELEASE:
            debounce_count++;
            if (debounce_count >= DEBOUNCE_TICKS) {
                bool pin_state = HAL_GPIO_ReadPin(GPIO_PORT, GPIO_PIN) == GPIO_PIN_SET;
                if (pin_state) {
                    if (click_up) *click_up = true;
                    if (click_callback) click_callback(*this);
                    state = IDLE;
                }
                else {
                    state = PRESSED;
                }
            }
            break;
        default:
            break;
        }
    }

    ButtonStateMachine(GPIO_TypeDef* GPIO_PORT, uint16_t GPIO_PIN, volatile bool* click_down = nullptr,
                       volatile bool* click_up = nullptr,
                       ButtonCallback click_callback = nullptr) : click_down(click_down), click_up(click_up),
                                                                  GPIO_PORT(GPIO_PORT), GPIO_PIN(GPIO_PIN),
                                                                  click_callback(click_callback) {
    }
};

class ButtonManager {
    static volatile ButtonStateMachine* buttons_[8];
    static volatile size_t count_;

public:
    static void registerButton(volatile ButtonStateMachine* btn) {
        if (count_ < 8) {
            buttons_[count_++] = btn;
        }
    }

    static void onTimerTick() {
        for (size_t i = 0; i < count_; i++) {
            buttons_[i]->onTimerTick();
        }
    }
};

volatile ButtonStateMachine* ButtonManager::buttons_[8] = {nullptr};
volatile size_t ButtonManager::count_ = 0;
bool redraw = false;

volatile ButtonStateMachine b1_state(GPIOC, GPIO_PIN_13, &input.enter, nullptr,
                                     [](const volatile ButtonStateMachine& button) {
                                         printf("click_down: %s\r\n",
                                                *button.click_down ? "true" : "false");
                                     });
volatile ButtonStateMachine b_up_state(GPIOA, GPIO_PIN_7, &input.up, nullptr,
                                       [](const volatile ButtonStateMachine& button) {
                                           printf("up called up: %s\r\n", *button.click_down ? "true" : "false");
                                       });
volatile ButtonStateMachine b_down_state(GPIOC, GPIO_PIN_4, &input.down, nullptr,
                                         [](const volatile ButtonStateMachine& button) {
                                             printf("down called up: %s\r\n", *button.click_down ? "true" : "false");
                                         });
volatile ButtonStateMachine b_break_out_state(GPIOC, GPIO_PIN_5, &input.break_out, nullptr,
                                              [](const volatile ButtonStateMachine& button) {
                                                  printf("break_out called up: %s\r\n",
                                                         *button.click_down ? "true" : "false");
                                              });
volatile ButtonStateMachine b_return_home_state(GPIOB, GPIO_PIN_0, &return_home, nullptr,
                                                [](const volatile ButtonStateMachine& button) {
                                                    printf("return_home called up: %s\r\n",
                                                           *button.click_down ? "true" : "false");
                                                });
volatile ButtonStateMachine b_shift_state(GPIOB, GPIO_PIN_2, &input.shift, nullptr,
                                          [](const volatile ButtonStateMachine& button) {
                                              printf("shift called up: %s\r\n",
                                                     *button.click_down ? "true" : "false");
                                          });

struct ButtonRegistrar {
    ButtonRegistrar() {
        ButtonManager::registerButton(&b1_state);
        ButtonManager::registerButton(&b_up_state);
        ButtonManager::registerButton(&b_down_state);
        ButtonManager::registerButton(&b_break_out_state);
        ButtonManager::registerButton(&b_return_home_state);
        ButtonManager::registerButton(&b_shift_state);
    }
} button_registrar;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config();
void directory_enum_test();
void sd_speed_test(UnicodeFont& font);
void spi_speed_test();
void pic_display_test(UnicodeFont& font);
void video_play_test();
void video_test2();
void menu_test();
void canvas_test(UnicodeFont& font);
void file_callback(const easy_menu::MenuCell* sender, easy_menu::ClickType type, void* user_data);
void shift_callback(const easy_menu::MenuCell* sender, void* user_data);
void open_file(const char* gbk_path);
void file_manager(const char* current_path = "/", uint32_t start_index = 0);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

struct MemNode {
    MemNode* next;
    bool data[28];
};

uint32_t measure_free_heap(bool print = true) {
    MemNode* head = nullptr;
    MemNode* current = nullptr;
    uint32_t total_allocated = 0;
    uint32_t node_count = 0;

    while (true) {
        auto* node = static_cast<MemNode*>(malloc(sizeof(MemNode)));
        if (!node) {
            break;
        }
        node->next = nullptr;
        total_allocated += sizeof(MemNode);
        node_count++;

        if (head == nullptr) {
            head = node;
            current = node;
        }
        else {
            current->next = node;
            current = node;
        }
    }

    current = head;
    while (current) {
        MemNode* next = current->next;
        free(current);
        current = next;
    }

    if (print) printf("堆内存检测: 申请 %lu 个节点, 共 %lu 字节\r\n", node_count, total_allocated);
    return total_allocated;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main() {
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSI_ENABLE();
    uint32_t tickstart = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET) {
        if ((HAL_GetTick() - tickstart) > LSI_TIMEOUT_VALUE) {
            // 如果 LSI 超时都没起来，说明芯片 LSI 坏了或者电路问题
            Error_Handler();
        }
    }
    /* USER CODE END Init */

    /* USER CODE BEGIN SysInit */
    SystemClock_Config();
    printf("System clock configured!\r\n");
    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM10_Init();
    MX_RTC_Init();
    MX_SDIO_SD_Init();
    MX_SPI2_Init();
    MX_USART2_UART_Init();
    MX_FATFS_Init();
    /* USER CODE BEGIN 2 */
    HAL_TIM_Base_Start_IT(&htim10);
    HAL_SD_CardInfoTypeDef cardInfo;
    if (HAL_SD_GetCardInfo(&hsd, &cardInfo) == HAL_OK) {
        printf("Card Type: %lu (", cardInfo.CardType);
        switch (cardInfo.CardType) {
        case 0: printf("SDSC)\r\n");
            break;
        case 1: printf("SDHC)\r\n");
            break;
        case 2: printf("SDXC)\r\n");
            break;
        default: printf("Unknown)\r\n");
            break;
        }
        const uint64_t capacity_bytes = static_cast<uint64_t>(cardInfo.LogBlockNbr) * cardInfo.LogBlockSize;
        const uint32_t capacity_mb = capacity_bytes / (1024 * 1024);

        printf("SD Card: %lu MB\r\n", capacity_mb);
    }
    else {
        printf("Failed to get SD card info!\r\n");
    }

    FRESULT res = f_mount(&SDFatFS, SDPath, 1);
    if (res == FR_OK) {
        printf("SD Card mounted successfully!\r\n");
    }
    else {
        printf("Failed to mount SD card: %d\r\n", res);
    }

    // directory_enum_test();

    ST7735_Init();
    ST7735_FillScreenFast(ST7735_BLACK);
    HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_SET);
    uint8_t offset = 0;
    ST7735_WriteString(0, offset, "Hello World!", Font_7x10, ST7735_GREEN, ST7735_YELLOW);
    ST7735_WriteStringNoBg(0, offset += 11, "Hello World!", Font_7x10, ST7735_GREEN);

    // 大字体，超过了1000字符缓存限制，不会分配索引缓存
    auto& font = global_font;
    if (font.Load("/font/WenQuanDianZhenZhengHei-1_12x12.ufnt")) {
        printf("字体large加载成功！\r\n");
        ST7735_Select();
        WriteUnicodeStringUTF8DMA(0, offset += font.GetDefaultHeight() + 1, "你好，世界！", &font, ST7735_GREEN,
                                  ST7735_YELLOW);
        WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, "你好，世界！", &font, ST7735_GREEN);
    }
    else {
        printf("字体large加载失败！\r\n");
    }

    measure_free_heap();
    HAL_Delay(3000);

    // sd_speed_test(font);
    // spi_speed_test();
    // pic_display_test(font);
    // measure_free_heap();
    // video_play_test();
    // video_test2();
    // measure_free_heap();
    // canvas_test(global_font);
    // measure_free_heap();
    // menu_test();
    // measure_free_heap();
    ST7735_FillScreenFast(ST7735_BLACK);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    easy_menu::StaticMenu root(2, "MP4播放器", 0, 0, 160, 128);
    root.add_menu("设置", [](const easy_menu::MenuCell* sender, easy_menu::ClickType type, void* userdata) {
        ST7735_FillScreenFast(ST7735_BLACK);
        WriteUnicodeStringUTF8DMA(0, 0, "暂不支持此功能", &global_font, ST7735_GREEN, ST7735_BLACK);
        while (!input.break_out and !return_home);
        input.break_out = false;
    });
    root.add_menu("文件浏览", [](const easy_menu::MenuCell* sender, easy_menu::ClickType type, void* userdata) {
        file_manager();
    });
    easy_menu::MenuState state;
    while (true) {
        /* USER CODE END WHILE */
        easy_menu::flush_menu(root, input, render, state);
        if (redraw) {
            root.force_redraw();
            redraw = false;
            printf("redraw\r\n");
        }
        if (return_home) {
            return_home = false;
            root.set_to_home();
        }
        /* USER CODE BEGIN 3 */
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config() {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 96;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == B1_Pin) {
        bool pin_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET;
        b1_state.onInterrupt(pin_state);
    }
    else if (GPIO_Pin == GPIO_PIN_7) {
        bool pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_SET;
        b_up_state.onInterrupt(pin_state);
    }
    else if (GPIO_Pin == GPIO_PIN_4) {
        bool pin_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == GPIO_PIN_SET;
        b_down_state.onInterrupt(pin_state);
    }
    else if (GPIO_Pin == GPIO_PIN_5) {
        bool pin_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_SET;
        b_break_out_state.onInterrupt(pin_state);
    }
    else if (GPIO_Pin == GPIO_PIN_0) {
        bool pin_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET;
        b_return_home_state.onInterrupt(pin_state);
    }
    else if (GPIO_Pin == GPIO_PIN_2) {
        bool pin_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET;
        b_shift_state.onInterrupt(pin_state);
    }
}

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM10) {
        ButtonManager::onTimerTick();
    }
}

extern "C" int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, reinterpret_cast<uint8_t*>(&ch), 1, HAL_MAX_DELAY);
    return ch;
}

extern "C" int __io_getchar(void) {
    uint8_t ch;
    HAL_UART_Receive(&huart2, &ch, 1, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, &ch, 1, HAL_MAX_DELAY);
    return ch;
}

extern "C" int fputc(int ch, FILE* f) {
    (void)f;
    return __io_putchar(ch);
}

extern "C" int _read(int file, char* ptr, int len) {
    (void)file; // 避免未使用参数的警告
    if (len == 0) {
        return 0;
    }

    // 调用我们已有的 __io_getchar 来获取一个字符
    int ch = __io_getchar();

    // 如果读到有效字符
    if (ch >= 0) {
        *ptr = static_cast<char>(ch);
        return 1; // 返回成功读取了1个字节
    }

    // 否则返回错误
    return -1;
}

void directory_enum(const char* path) {
    printf("枚举%s\r\n", path);
    for (auto&& obj : fs::listdir(path, false)) {
        char gbk_path[256];
        snprintf(gbk_path, sizeof(gbk_path), "%s/%s", path, obj.name);
        char unicode_path[256];
        fs::gbk_to_utf8(gbk_path, unicode_path, sizeof(unicode_path));
        if (obj.type == fs::dir) {
            printf("[DIR]  %s\r\n", unicode_path);
        }
        else {
            printf("[FILE] %s\t\t", unicode_path);
            FIL f;
            FRESULT res = f_open(&f, gbk_path, FA_READ);
            if (res == FR_OK) {
                FSIZE_t size = f_size(&f);
                printf("%lu\r\n", static_cast<unsigned long>(size));
                f_close(&f);
            }
            else {
                printf("打开失败: %d\r\n", res);
            }
        }
    }
}

void directory_enum_test() {
    directory_enum("/");
    directory_enum("/font");
    directory_enum("/pic");
    directory_enum("/video");
}

void sd_speed_test(UnicodeFont& font) {
    printf("\r\n=== SD卡读写速度测试 ===\r\n");
    ST7735_FillScreenFast(ST7735_BLACK);
    int offset = -font.GetDefaultHeight();
    WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, "正在进行SD卡读写速度测试", &font, ST7735_GREEN);

    static uint8_t buffer[4096];
    for (uint16_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] = static_cast<uint8_t>(i & 0xFF);
    }

    constexpr uint32_t test_size = 16 * 1024 * 1024;
    const auto test_file = "/speedtest.bin";

    printf("测试文件大小: %lu KB\r\n", test_size / 1024);

    printf("\r\n--- 写入测试 ---\r\n");
    FIL f;
    FRESULT res = f_open(&f, test_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("创建文件失败: %d\r\n", res);
        return;
    }

    UINT bytes_written;
    uint32_t total_written = 0;
    uint32_t start_tick = HAL_GetTick();

    while (total_written < test_size) {
        res = f_write(&f, buffer, sizeof(buffer), &bytes_written);
        if (res != FR_OK || bytes_written == 0) {
            printf("写入错误: %d\r\n", res);
            break;
        }
        total_written += bytes_written;
    }

    f_sync(&f);
    uint32_t write_elapsed_ms = HAL_GetTick() - start_tick;
    f_close(&f);

    char s1[64], s2[64];
    snprintf(s1, sizeof(s1), "写入完成: %lu 字节", total_written);
    snprintf(s2, sizeof(s2), "写入耗时: %lu ms", write_elapsed_ms);
    printf("%s\r\n%s\r\n", s1, s2);
    WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, s1, &font, ST7735_GREEN);
    WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, s2, &font, ST7735_GREEN);
    if (write_elapsed_ms > 0) {
        uint32_t write_speed = total_written / write_elapsed_ms;
        snprintf(s1, sizeof(s1), "写入速度: %lu KB/s", write_speed);
        printf("%s\r\n", s1);
        WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, s1, &font, ST7735_GREEN);
    }

    printf("\r\n--- 读取测试 ---\r\n");
    res = f_open(&f, test_file, FA_READ);
    if (res != FR_OK) {
        printf("打开文件失败: %d\r\n", res);
        return;
    }

    UINT bytes_read;
    uint32_t total_read = 0;
    start_tick = HAL_GetTick();

    while (total_read < test_size) {
        res = f_read(&f, buffer, sizeof(buffer), &bytes_read);
        if (res != FR_OK || bytes_read == 0) {
            printf("读取错误: %d\r\n", res);
            break;
        }
        total_read += bytes_read;
    }

    uint32_t read_elapsed_ms = HAL_GetTick() - start_tick;
    f_close(&f);

    snprintf(s1, sizeof(s1), "读取完成: %lu 字节", total_read);
    snprintf(s2, sizeof(s2), "读取耗时: %lu ms", read_elapsed_ms);
    printf("%s\r\n%s\r\n", s1, s2);
    WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, s1, &font, ST7735_GREEN);
    WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, s2, &font, ST7735_GREEN);
    if (read_elapsed_ms > 0) {
        uint32_t read_speed = total_read / read_elapsed_ms;
        snprintf(s1, sizeof(s1), "读取速度: %lu KB/s", read_speed);
        printf("%s\r\n", s1);
        WriteUnicodeStringUTF8NoBgDMA(0, offset += font.GetDefaultHeight() + 1, s1, &font, ST7735_GREEN);
    }

    printf("\r\n--- 数据校验 ---\r\n");
    res = f_open(&f, test_file, FA_READ);
    if (res == FR_OK) {
        bool verify_ok = true;
        uint32_t verify_pos = 0;
        while (verify_pos < test_size) {
            res = f_read(&f, buffer, sizeof(buffer), &bytes_read);
            if (res != FR_OK || bytes_read == 0) break;

            for (UINT i = 0; i < bytes_read; i++) {
                auto expected = static_cast<uint8_t>((verify_pos + i) & 0xFF);
                if (buffer[i] != expected) {
                    printf("校验失败 @偏移 %lu: 期望 0x%02X, 实际 0x%02X\r\n",
                           verify_pos + i, expected, buffer[i]);
                    verify_ok = false;
                    break;
                }
            }
            verify_pos += bytes_read;
            if (!verify_ok) break;
        }
        f_close(&f);
        printf("数据校验: %s\r\n", verify_ok ? "通过" : "失败");
    }

    printf("\r\n--- 清理测试文件 ---\r\n");
    res = f_unlink(test_file);
    printf("删除测试文件: %s\r\n", res == FR_OK ? "成功" : "失败");

    printf("\r\n=== 测试完成 ===\r\n\r\n");
    HAL_Delay(1000);
}

void spi_speed_test() {
    printf("\r\n=== SPI 速度测试 ===\r\n");

    uint16_t color_list[] = {ST7735_RED, ST7735_GREEN, ST7735_BLUE, ST7735_YELLOW};
    constexpr int test_frames = 100;

    struct {
        uint32_t prescaler;
        const char* name;
        uint32_t freq_khz;
    } speeds[] = {
        // {SPI_BAUDRATEPRESCALER_256, "256", 187},
        // {SPI_BAUDRATEPRESCALER_128, "128", 375},
        // {SPI_BAUDRATEPRESCALER_64,  "64",  750},
        // {SPI_BAUDRATEPRESCALER_32,  "32",  1500},
        //{SPI_BAUDRATEPRESCALER_16,  "16",  3000},
        {SPI_BAUDRATEPRESCALER_8, "8", 6000},
        {SPI_BAUDRATEPRESCALER_4, "4", 12000},
        {SPI_BAUDRATEPRESCALER_2, "2", 24000},
    };

    for (const auto& [prescaler, name, freq_khz] : speeds) {
        printf("\r\n--- SPI 分频 %s (%lu kHz) ---\r\n", name, freq_khz);

        MODIFY_REG(hspi2.Instance->CR1, SPI_CR1_BR, prescaler);
        HAL_Delay(10);

        ST7735_FillScreenFast(ST7735_BLACK);
        HAL_Delay(10);

        uint32_t start_tick = HAL_GetTick();
        for (int i = 0; i < test_frames; i++) {
            ST7735_FillScreenFast(color_list[i % 4]);
        }
        uint32_t elapsed = HAL_GetTick() - start_tick;

        double fps = test_frames * 1000.0 / elapsed;
        double kbps = test_frames * 40.96 / elapsed * 1000;

        printf("刷屏 %d 次, 耗时 %lu ms\r\n", test_frames, elapsed);
        printf("帧率: %.2f fps, 带宽: %.0f KB/s\r\n", fps, kbps);

        HAL_Delay(500);
    }

    MODIFY_REG(hspi2.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_2);
    printf("\r\n=== 测试完成，已恢复最高速度 ===\r\n\r\n");
}

void pic_display_test(UnicodeFont& font) {
    // const char s1[] = "静态加载展示";
    constexpr char s2[] = "动态加载展示";

    // ST7735_DrawImage(0, 0, 160, 128, *xilian);
    // WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(s1, &font)) / 2, 100, s1, &font, ST7735_GREEN, ST7735_BLACK);
    // HAL_Delay(3000);

    // 测试：从SD卡动态加载并显示图片
    ST7735_FillScreenFast(ST7735_BLACK);
    int offset = 80 - font.GetDefaultHeight();
    auto tick = HAL_GetTick();
    DynamicImage image("/pic/cyrene.bmp");
    if (image.IsLoaded()) {
        printf("正常加载bmp耗时 %lu tick\r\n", HAL_GetTick() - tick);
        tick = HAL_GetTick();
        image.DisplayDMA(0, 0);
        printf("正常显示bmp耗时 %lu tick\r\n", HAL_GetTick() - tick);
        // measure_free_heap();
        char title[] = "昔涟";
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(s2, &font)) / 2, offset += font.GetDefaultHeight() + 1,
                                  s2, &font, ST7735_GREEN, ST7735_BLACK);
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(title, &font)) / 2,
                                  offset += font.GetDefaultHeight() + 1, title, &font, ST7735_GREEN, ST7735_BLACK);
    }
    else {
        printf("图片cyrene.bmp显示失败%s\r\n", image.GetErrorString());
    }

    HAL_Delay(3000);

    ST7735_FillScreenFast(ST7735_BLACK);
    offset = 80 - font.GetDefaultHeight();
    tick = HAL_GetTick();
    auto rst = PIC_DisplayStreamingDMA("/pic/evernight.bmp", 0, 0, 0, 0, 160, 128);
    printf("流式显示bmp耗时 %lu tick\r\n", HAL_GetTick() - tick);
    // measure_free_heap();
    if (rst == PIC_SUCCESS) {
        char title[] = "长夜月";
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(s2, &font)) / 2, offset += font.GetDefaultHeight() + 1,
                                  s2, &font, ST7735_GREEN, ST7735_BLACK);
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(title, &font)) / 2,
                                  offset += font.GetDefaultHeight() + 1, title, &font, ST7735_GREEN, ST7735_BLACK);
    }
    else {
        printf("图片evernight.bmp显示失败：%s\r\n", image.GetErrorString());
    }

    HAL_Delay(3000);

    ST7735_FillScreenFast(ST7735_BLACK);
    offset = 80 - font.GetDefaultHeight();
    tick = HAL_GetTick();
    if (image.LoadFromSD("/pic/castorice.jpg")) {
        printf("正常加载jpg耗时 %lu tick\r\n", HAL_GetTick() - tick);
        tick = HAL_GetTick();
        image.DisplayDMA(0, 0);
        printf("正常显示jpg耗时 %lu tick\r\n", HAL_GetTick() - tick);
        printf("峰值内存检测：");
        measure_free_heap();
        char title[] = "遐蝶";
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(s2, &font)) / 2, offset += font.GetDefaultHeight() + 1,
                                  s2, &font, ST7735_GREEN, ST7735_BLACK);
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(title, &font)) / 2,
                                  offset += font.GetDefaultHeight() + 1, title, &font, ST7735_GREEN, ST7735_BLACK);
    }
    else {
        printf("图片castorice.jpg显示失败：%s\r\n", image.GetErrorString());
    }

    HAL_Delay(3000);

    ST7735_FillScreenFast(ST7735_BLACK);
    offset = 80 - font.GetDefaultHeight();
    tick = HAL_GetTick();
    rst = PIC_DisplayStreamingDMA("/pic/hyacine.jpg", 0, 0, 0, 0, 160, 128);
    printf("流式显示jpg耗时 %lu tick\r\n", HAL_GetTick() - tick);
    // measure_free_heap();
    if (rst == PIC_SUCCESS) {
        char title[] = "风瑾";
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(s2, &font)) / 2, offset += font.GetDefaultHeight() + 1,
                                  s2, &font, ST7735_GREEN, ST7735_BLACK);
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(title, &font)) / 2,
                                  offset += font.GetDefaultHeight() + 1, title, &font, ST7735_GREEN, ST7735_BLACK);
    }
    else {
        printf("图片hyacine.jpg显示失败：%s\r\n", PIC_GetErrorString(rst));
    }

    HAL_Delay(3000);

    ST7735_FillScreenFast(ST7735_BLACK);
    offset = 80 - font.GetDefaultHeight();
    tick = HAL_GetTick();
    char xilian_path[64];
    fs::utf8_to_gbk("/pic/昔涟.jpg", xilian_path, sizeof(xilian_path));
    rst = PIC_DisplayStreamingDMA(xilian_path, 0, 0, 0, 0, 160, 128);
    printf("流式显示jpg耗时 %lu tick\r\n", HAL_GetTick() - tick);
    // measure_free_heap();
    if (rst == PIC_SUCCESS) {
        char title[] = "昔涟";
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(s2, &font)) / 2, offset += font.GetDefaultHeight() + 1,
                                  s2, &font, ST7735_GREEN, ST7735_BLACK);
        WriteUnicodeStringUTF8DMA((160 - UnicodeStringUTF8Length(title, &font)) / 2,
                                  offset += font.GetDefaultHeight() + 1, title, &font, ST7735_GREEN, ST7735_BLACK);
    }
    else {
        printf("图片昔涟.jpg显示失败：%s\r\n", PIC_GetErrorString(rst));
    }

    HAL_Delay(3000);
}

void print_video_info(const VideoPlayer& player) {
    VideoInfo info;
    if (!player.GetInfo(&info)) {
        printf("信息获取错误: %s\r\n", player.GetErrorString());
        return;
    }
    printf("视频大小: %dx%d\r\n", info.width, info.height);
    printf("帧率: %d\r\n", info.fps);
    printf("帧数: %lu\r\n", info.total_frames);
    printf("时长: %lu ms\r\n", info.duration_ms);
    printf("格式: ");
    switch (info.format) {
    case VIDEO_FORMAT_UNKNOWN:
        printf("未知");
        break;
    case VIDEO_FORMAT_MJPEG:
        printf("MJPEG");
        break;
    case VIDEO_FORMAT_RAW_RGB565:
        printf("RGB565");
        break;
    case VIDEO_FORMAT_RAW_RGB565_LE:
        printf("RGB565-LE");
        break;
    case VIDEO_FORMAT_RAW_RGB565_BE:
        printf("RGB565-BE");
        break;
    case VIDEO_FORMAT_RAW_RGB888:
        printf("RGB888");
        break;
    }
    printf("\r\n");
    // printf("大小: %lu bytes\r\n", info.file_size);
}

void print_video_display_info(const VideoPlayer& player) {
    printf("渲染帧数: %lu\r\n", player.GetFramesRendered());
    printf("跳帧数: %lu\r\n", player.GetFramesSkipped());
    printf("平均帧率: %.2f fps\r\n", player.GetAverageFps());
}

void video_play_test() {
    VideoPlayer player;
    uint32_t start_tick;

    // player = VideoPlayer("/video/bad apple(mjpg).avi");
    // print_video_info(player);
    // start_tick = HAL_GetTick();
    // if (!player.Play(0, 0)) {
    //     printf("播放错误: %s\r\n", player.GetErrorString());
    // }
    // printf("播放用时 %lu ms\r\n", HAL_GetTick() - start_tick);
    // print_video_display_info(player);

    for (auto&& obj : fs::listdir("/video", false)) {
        if (obj.type == fs::file) {
            if (!fs::suffix_matches(obj.name, ".avi")) {
                continue;
            }
            char full_path[64];
            snprintf(full_path, sizeof(full_path), "/video/%s", obj.name);
            char unicode_path[64];
            fs::gbk_to_utf8(full_path, unicode_path, sizeof(unicode_path));
            printf("即将播放%s\r\n", unicode_path);
            player = VideoPlayer(full_path);
            print_video_info(player);
            VideoInfo info;
            player.GetInfo(&info);
            auto width = info.width, height = info.height;
            ST7735_FillScreenFast(ST7735_BLACK);
            start_tick = HAL_GetTick();
            if (!player.Play((160 - width) / 2, (128 - height) / 2)) {
                printf("播放错误: %s\r\n", player.GetErrorString());
            }
            printf("播放用时 %lu ms\r\n", HAL_GetTick() - start_tick);
            print_video_display_info(player);
        }
    }
}

void video_test2() {
    VideoPlayer player = VideoPlayer("/video/bad apple(mjpg).avi");
    print_video_info(player);
    uint32_t start_tick = HAL_GetTick();
    player.Play(0, 0, VIDEO_PLAY_MODE_POLLING);
    while (player.GetState() == VIDEO_STATE_PLAYING) {
        bool err = player.Poll();
        if (!err) {
            printf("播放错误: %s\r\n", VIDEO_GetErrorString(VIDEO_GetLastError()));
            break;
        }
    }
    printf("播放用时 %lu ms\r\n", HAL_GetTick() - start_tick);
    print_video_display_info(player);
}

void menu_test() {
    using namespace easy_menu;
    ST7735_FillScreenFast(ST7735_YELLOW);
    global_canvas.FillCanvas(ST7735_BLACK);
    printf("menu test\r\n");
    DynamicMenu root("标题", 0, 0, 160, 128);
    root.add_menu("选项1", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项1, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    auto submenu = new StaticMenu(1, "这是子菜单", 0, 0, 160, 128);
    submenu->add_menu("子菜单选项1", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了子菜单选项1, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项2", submenu);
    root.add_menu("选项3", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项3, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项4", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项4, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项5", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项5, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项6", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项6, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项7", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项7, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项8", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项8, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项9", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项9, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项10", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项10, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项11", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项11, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项12", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项12, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项13", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项13, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });
    root.add_menu("选项14", [](const MenuCell* cell, ClickType click, void* data) {
        printf("点击了选项14, 点击类型: %s\r\n", click == ENTER ? "ENTER" : "SHIFT");
    });

    bool breakout = false;
    MenuState menu_state;
    root.add_menu("退出菜单测试", [](const MenuCell* cell, ClickType click, void* data) { *static_cast<bool*>(data) = true; },
                  &breakout);
    while (!breakout) {
        auto start = HAL_GetTick();
        if (!flush_menu(root, input, render, menu_state)) breakout = true;
        if (return_home) {
            return_home = false;
            root.set_to_home();
        }
        printf("渲染菜单帧耗时 %lu ms\r\n", HAL_GetTick() - start);
    }
    printf("已退出根菜单\r\n");
}

void canvas_test(UnicodeFont& font) {
    // printf("canvas创建前");
    // measure_free_heap();
    // Canvas canvas(160, 128);
    // printf("canvas创建后");
    // measure_free_heap();
    Canvas& canvas = global_canvas;
    canvas.FillCanvas(ST7735_WHITE);
    uint32_t start_tick = HAL_GetTick();
    canvas.DrawCanvasDMA();
    printf("DMA传输耗时 %lu ms\r\n", HAL_GetTick() - start_tick);
    HAL_Delay(1000);
    canvas.Line(0, 0, 160, 128, ST7735_YELLOW);
    canvas.DrawCanvasDMA();
    HAL_Delay(1000);

    {
        DynamicImage image("/pic/cyrene.bmp");
        measure_free_heap();
        start_tick = HAL_GetTick();
        canvas.DrawImage(image, 138, 110, 70, 47, 22, 18);
        printf("绘制图片耗时 %lu ms\r\n", HAL_GetTick() - start_tick);
    }
    measure_free_heap();
    canvas.DrawCanvasDMA();
    HAL_Delay(1000);

    start_tick = HAL_GetTick();
    int offset = 60 - font.GetDefaultHeight() - 1;
    canvas.WriteUnicodeString(0, offset += font.GetDefaultHeight() + 1, "12345", &font, ST7735_GREEN, ST7735_BLACK);
    canvas.WriteUnicodeString(0, offset += font.GetDefaultHeight() + 1, "ABCDE", &font, ST7735_GREEN);
    canvas.WriteUnicodeString(0, offset += font.GetDefaultHeight() + 1, "abcde", &font, ST7735_GREEN, ST7735_BLACK);
    canvas.WriteUnicodeString(0, offset += font.GetDefaultHeight() + 1, "「你好，世界♪」", &font, ST7735_GREEN, ST7735_YELLOW);
    printf("绘制文本耗时 %lu ms\r\n", HAL_GetTick() - start_tick);
    canvas.DrawCanvasDMA();
    measure_free_heap();
    HAL_Delay(2000);

    start_tick = HAL_GetTick();
    canvas.FillCircle(10, 10, 7, ST7735_RED);
    canvas.HollowCircle(10, 30, 7, ST7735_RED);
    canvas.FillRectangle(20, 0, 10, 10, ST7735_YELLOW);
    canvas.HollowRectangle(20, 20, 10, 10, ST7735_YELLOW);
    canvas.HollowRectangle(40, 0, 10, 10, ST7735_YELLOW);
    canvas.HollowRectangle(40, 20, 10, 10, ST7735_YELLOW);
    canvas.FillTriangle(40, 0, 45, 10, 50, 5, ST7735_BLUE);
    canvas.HollowTriangle(40, 20, 45, 30, 50, 25, ST7735_BLUE);
    canvas.HollowRectangle(60, 2, 10, 16, ST7735_YELLOW);
    canvas.HollowRectangle(60, 22, 10, 16, ST7735_YELLOW);
    canvas.FillEllipse(65, 10, 5, 8, ST7735_GREEN);
    canvas.HollowEllipse(65, 30, 5, 8, ST7735_GREEN);
    printf("绘制图形耗时 %lu ms\r\n", HAL_GetTick() - start_tick);
    canvas.DrawCanvasDMA();
    measure_free_heap();
    HAL_Delay(2000);

    start_tick = HAL_GetTick();
    canvas.Copy(0, 0, 80, 60, 80, 0);
    printf("复制耗时 %lu ms\r\n", HAL_GetTick() - start_tick);
    canvas.DrawCanvasDMA();
    measure_free_heap();
    HAL_Delay(2000);

    canvas.Copy(0, 20, 80, 60, 0, 0);
    canvas.DrawCanvasDMA();
    HAL_Delay(2000);
}

void file_callback(const easy_menu::MenuCell* sender, easy_menu::ClickType type, void* user_data) {
    if (type == easy_menu::ENTER) {
        char gbk_path[256];
        {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", *static_cast<char**>(user_data), sender->title);
            fs::utf8_to_gbk(full_path, gbk_path, sizeof(gbk_path));
        }
        open_file(gbk_path);
    }
    else {
        shift_callback(sender, user_data);
    }
}

void shift_callback(const easy_menu::MenuCell* sender, void* user_data) {
    ST7735_FillScreenFast(ST7735_BLACK);
    WriteUnicodeStringUTF8DMA(0, 0, "暂不支持此功能", &global_font, ST7735_GREEN, ST7735_BLACK);
    while (!input.break_out and !return_home);
    input.break_out = false;
}

void open_file(const char* gbk_path) {
    ST7735_FillScreenFast(ST7735_BLACK);
    if (fs::suffix_matches(gbk_path, ".avi")) {
        VideoPlayer player(gbk_path);
        VideoInfo info;
        player.GetInfo(&info);
        player.Play((160 - info.width) / 2, (128 - info.height) / 2, VIDEO_PLAY_MODE_POLLING);
        bool paused = false;
        while (player.GetState() == VIDEO_STATE_PLAYING) {
            if (input.enter) {
                input.enter = false;
                paused = !paused;
                if (!paused) player.ResetTime();
            }
            if (!paused) {
                bool playing = player.Poll();
                if (!playing or input.break_out or return_home) {
                    input.break_out = false;
                    break;
                }
            }
            else {
                if (input.break_out or return_home) {
                    input.break_out = false;
                    break;
                }
            }
        }
        return;
    }
    else if (fs::suffix_matches(gbk_path, ".bmp") || fs::suffix_matches(gbk_path, ".jpg") || fs::suffix_matches(
        gbk_path, ".raw")) {
        PIC_DisplayStreamingDMA(gbk_path, 0, 0, 0, 0, 0, 0);
    }
    else {
        WriteUnicodeStringUTF8DMA(0, 0, "暂不支持此格式", &global_font, ST7735_GREEN, ST7735_BLACK);
    }
    while (!input.break_out and !return_home);
    input.break_out = false;
}

void file_manager(const char* current_path, uint32_t start_index) {
    struct PublicData {
        bool is_dir[20] = {false};
        const char* path = nullptr;
        const char* current = nullptr;
    };
    struct Data {
        PublicData& data;
        int index = 0;
    };
    A:
    PublicData data;
    bool next_page = false;
    data.current = current_path;
    char* path = nullptr;
    {
        char names[20][256];
        int len = -1;
        {
            auto iter = fs::listdir(current_path).begin() + start_index;
            for (int i = 0; i < 20; i++) {
                if (iter != fs::DirectoryRange::end()) {
                    strcpy(names[i], (*iter).name);
                    data.is_dir[i] = (*iter).type == fs::ObjectType::dir;
                }
                else {
                    len = i;
                    break;
                }
                ++iter;
            }
        }
        bool end = len != -1;
        if (!end) len = 20;
        easy_menu::StaticMenu menu(len, current_path, 0, 0, 160, 128);
        for (int i = 0; i < len; i++) {
            auto temp = new Data({data, i});
            menu.add_menu(names[i], [](const easy_menu::MenuCell* sender, easy_menu::ClickType type, void* user_data) {
                auto data = static_cast<Data*>(user_data);
                if (data->data.is_dir[data->index]) {
                    data->data.path = sender->title;
                    input.break_out = true;
                }
                else {
                    file_callback(sender, type, &data->data.current);
                }
            }, temp);
        }
        if (!end) {
            menu.add_menu("加载下一页", [](const easy_menu::MenuCell* sender, easy_menu::ClickType type, void* user_data) {
                *static_cast<bool*>(user_data) = true;
                input.break_out = true;
            }, &next_page);
        }
        easy_menu::MenuState state;
        while (easy_menu::flush_menu(menu, input, render, state) and not return_home);
        printf("1");
        if (data.path) {
            path = new char[256];
            snprintf(path, 256 * sizeof(char), "%s/%s", current_path, data.path);
        }
    }
    if (path) {
        if (!return_home) file_manager(path);
        delete[] path;
        if (!return_home) goto A;
    }
    if (next_page) {
        file_manager(current_path, start_index + 20);
    }
    redraw = true;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler() {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    printf("Error Handler called!\r\n");
    while (true) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(100);
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
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
