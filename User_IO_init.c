#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "soc/soc.h"
#include "soc/gpio_periph.h"
#include "driver/gpio.h"
#include "driver/gpio_filter.h"
#include "esp_attr.h"
#include "esp_log.h"

#include "User_main.h"
#include "User_taskManagement.h"
#include "sdkconfig.h"
#include "User_IO_init.h"
#define ISR_DEBUG 0
#define ESP_INTR_FLAG_DEFAULT 0
#define NOT_SUPPORTED_glitch_filter 0

#define SET_BIT(SRC, BIT) (SRC |= (1 << BIT))
#define CLEAR_BIT(SRC, BIT) (SRC &= ~(1 << BIT))

static const char *TAG = "User_IO";

#if NOT_SUPPORTED_glitch_filter
// GPIO毛刺过滤器句柄
static gpio_glitch_filter_handle_t My_GPIO_filter_handler;
#endif

// 中断队列(指针)
static QueueSetHandle_t gpioISR_evt_queue = NULL;

#if ISR_DEBUG
// 中断触发次数
static volatile unsigned char handler_count = 0;
#endif

// (DP)GFEDCBA
//    00000000
// 记录四位数码管的状态, 第0位不做计数, 如果操作了第0位则证明操作是无效的
static volatile uint8_t My_LED_status[5] = {0};

static uint8_t clockbell_level = false;

/// @brief GPIO中断处理
/// @note IRAM_ATTR
/// @param arg
/// @return
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
#if ISR_DEBUG
    // 中断任务内部不执行打印
    handler_count++;
#endif
    uint32_t gpio_num = (uint32_t)arg;

    // 从中断任务中发送数据到任务队列
    xQueueSendFromISR(gpioISR_evt_queue, &gpio_num, NULL);
    return;
}

/// @brief GPIO中断处理接收任务
/// @param arg
static void gpio_isr_handler_receive_task(void *arg)
{
    uint32_t io_num = 0;
    while (1)
    {
        if (xQueueReceive(gpioISR_evt_queue, &io_num, portMAX_DELAY))
        {
#if ISR_DEBUG
            printf("%s GPIO[%" PRIu32 "] intr, val: %d, handler count: %d\n",
                   __func__, io_num, gpio_get_level(io_num), handler_count);
#endif
            clockbell_level = (uint8_t)gpio_get_level(io_num);
        }
    }
}

/**
 * @brief  段位开关
 *
 * @param  IO_num 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'(dp)
 * @param  level 0, 1
 * @note 段位开关, 0打开, 1关闭
 *
 */
#if (LED_DRIVER_VERSION_MAJOR != LED_TASK_VERSION)
ISR_SAFE
#endif
static inline void User_LED_on(const bool level, const int IO_num)
{
    uint32_t IO = REG_READ(GPIO_OUT_REG);

    switch (IO_num)
    {
    case 'A':
        (level) ? SET_BIT(IO, GPIO_NUM_17) : CLEAR_BIT(IO, GPIO_NUM_17);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_17, level); // A段
        break;
    case 'B':
        (level) ? SET_BIT(IO, GPIO_NUM_15) : CLEAR_BIT(IO, GPIO_NUM_15);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_15, level); // B段
        break;
    case 'C':
        (level) ? SET_BIT(IO, GPIO_NUM_27) : CLEAR_BIT(IO, GPIO_NUM_27);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_27, level); // C段
        break;
    case 'D':
        (level) ? SET_BIT(IO, GPIO_NUM_12) : CLEAR_BIT(IO, GPIO_NUM_12);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_12, level); // D段
        break;
    case 'E':
        (level) ? SET_BIT(IO, GPIO_NUM_13) : CLEAR_BIT(IO, GPIO_NUM_13);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_13, level); // E段
        break;
    case 'F':
        (level) ? SET_BIT(IO, GPIO_NUM_16) : CLEAR_BIT(IO, GPIO_NUM_16);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_16, level); // F段
        break;
    case 'G':
        (level) ? SET_BIT(IO, GPIO_NUM_26) : CLEAR_BIT(IO, GPIO_NUM_26);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_26, level); // G段
        break;
    case 'H':
        (level) ? SET_BIT(IO, GPIO_NUM_14) : CLEAR_BIT(IO, GPIO_NUM_14);
        REG_WRITE(GPIO_OUT_REG, IO);
        // err = gpio_set_level(GPIO_NUM_14, level); // dp段
        break;
    default:
        ESP_LOGE(__func__, "LED IO_num error");
        break;
    }
    return;
}

/**
 * @brief  数码管开关
 *
 * @param  IO_num 0(中间两点), 1, 2, 3, 4
 * @param  level 0, 1
 *
 */
#if (LED_DRIVER_VERSION_MAJOR != LED_TASK_VERSION)
ISR_SAFE
#endif
static inline void User_LED_digital_on(const bool level, const int IO_num)
{
#if !CONFIG_GPIO_CTRL_FUNC_IN_IRAM
    uint32_t IO = REG_READ(GPIO_OUT_REG);
    static const uint32_t gpio_33 = (GPIO_NUM_33 - GPIO_NUM_32);
#endif
    switch (IO_num)
    {
    case 0:
#if !CONFIG_GPIO_CTRL_FUNC_IN_IRAM
        IO = REG_READ(GPIO_OUT1_REG);
        (level) ? SET_BIT(IO, gpio_33) : CLEAR_BIT(IO, gpio_33);
        REG_WRITE(GPIO_OUT1_REG, IO << gpio_33);
#else
        gpio_set_level(GPIO_NUM_33, level); // 中间两点
#endif
        break;
    case 1:
#if !CONFIG_GPIO_CTRL_FUNC_IN_IRAM
        (level) ? SET_BIT(IO, GPIO_NUM_5) : CLEAR_BIT(IO, GPIO_NUM_5);
        REG_WRITE(GPIO_OUT_REG, IO << GPIO_NUM_5);
#else
        gpio_set_level(GPIO_NUM_5, level); // digital_1
#endif
        break;
    case 2:
#if !CONFIG_GPIO_CTRL_FUNC_IN_IRAM
        (level) ? SET_BIT(IO, GPIO_NUM_4) : CLEAR_BIT(IO, GPIO_NUM_4);
        REG_WRITE(GPIO_OUT_REG, IO << GPIO_NUM_4);
#else
        gpio_set_level(GPIO_NUM_4, level); // digital_2
#endif
        break;
    case 3:
#if !CONFIG_GPIO_CTRL_FUNC_IN_IRAM
        (level) ? SET_BIT(IO, GPIO_NUM_2) : CLEAR_BIT(IO, GPIO_NUM_2);
        REG_WRITE(GPIO_OUT_REG, IO << GPIO_NUM_2);
#else
        gpio_set_level(GPIO_NUM_2, level); // digital_3
#endif
        break;
    case 4:
#if !CONFIG_GPIO_CTRL_FUNC_IN_IRAM
        (level) ? SET_BIT(IO, GPIO_NUM_25) : CLEAR_BIT(IO, GPIO_NUM_25);
        REG_WRITE(GPIO_OUT_REG, IO << GPIO_NUM_25);
#else
        gpio_set_level(GPIO_NUM_25, level); // digital_4
#endif
        break;
    default:
        ESP_LOGE(TAG, "digital IO_num error");
        break;
    }

    return;
}

/**
 * @brief  GPIO初始化
 *
 *
 */
void User_gpio_init(void)
{

    // GPIO结构体
    gpio_config_t My_GPIO_structture = {
        .pin_bit_mask = (1ULL << 32),     // GPIO32, 1左移32bit
        .mode = GPIO_MODE_OUTPUT,         // 输出模式
        .pull_up_en = GPIO_PULLUP_ENABLE, // 上拉电阻, pull up; 下拉电阻, pull down
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // bell
    ESP_ERROR_CHECK(gpio_set_drive_capability(GPIO_NUM_32, GPIO_DRIVE_CAP_DEFAULT));

    My_GPIO_structture.pin_bit_mask = (1ULL << 5);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(DIG1)
    ESP_ERROR_CHECK(gpio_set_drive_capability(GPIO_NUM_5, GPIO_DRIVE_CAP_DEFAULT));

    My_GPIO_structture.pin_bit_mask = (1ULL << 4);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(DIG2)
    ESP_ERROR_CHECK(gpio_set_drive_capability(GPIO_NUM_4, GPIO_DRIVE_CAP_DEFAULT));

    My_GPIO_structture.pin_bit_mask = (1ULL << 2);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(DIG3)
    ESP_ERROR_CHECK(gpio_set_drive_capability(GPIO_NUM_2, GPIO_DRIVE_CAP_DEFAULT));

    My_GPIO_structture.pin_bit_mask = (1ULL << 25);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(DIG4)
    ESP_ERROR_CHECK(gpio_set_drive_capability(GPIO_NUM_25, GPIO_DRIVE_CAP_DEFAULT));

    My_GPIO_structture.pin_bit_mask = (1ULL << 33);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(中间两点)


    My_GPIO_structture.mode = GPIO_MODE_OUTPUT;
    My_GPIO_structture.pin_bit_mask = (1ULL << 17);
    My_GPIO_structture.pull_down_en = GPIO_PULLDOWN_ENABLE;
    My_GPIO_structture.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(A段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 15);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(B段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 27);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(C段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 12);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(D段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 13);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(E段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 16);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(F段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 26);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(G段)

    My_GPIO_structture.pin_bit_mask = (1ULL << 14);
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 数码管(DP段)
    

#if NOT_SUPPORTED_glitch_filter
    // 配置GPIO毛刺过滤器结构体
    // GPIO毛刺过滤器结构体
    gpio_pin_glitch_filter_config_t My_GPIO_filter_structture = {
        .clk_src = 0,
        .gpio_num = GPIO_NUM_5,
    };

    // 为IO口配置输入去毛刺
    esp_err_t ret = gpio_new_pin_glitch_filter(&My_GPIO_filter_structture,
                                               &My_GPIO_filter_handler);

    // 启用去毛刺   ps: 启用去毛刺后, 效果显著, 用手拔插杜邦线触发中断时不会太多次连续触发
    ESP_ERROR_CHECK(gpio_glitch_filter_enable(My_GPIO_filter_handler));
    gpio_new_pin_glitch_filter(&My_GPIO_filter_structture, &My_GPIO_filter_handler);
#endif // esp32冇这个功能, esp32-s3有

    My_GPIO_structture.pin_bit_mask = (1ULL << 35);
    My_GPIO_structture.mode = GPIO_MODE_INPUT;
    My_GPIO_structture.intr_type = GPIO_INTR_POSEDGE;
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture));

    // 创建中断接收队列, 没有创建将会导致队列断言失败restart core
    gpioISR_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // install gpio isr service, 安装gpio中断服务
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));

    // hook isr handler for specific gpio pin, 添加gpio中断处理函数
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_35,
                                         gpio_isr_handler,
                                         (void *)GPIO_NUM_35));

    // start gpio_isr task, 创建gpio中断处理接收任务
    xTaskCreatePinnedToCore(gpio_isr_handler_receive_task,
                            GPIO_ISR_TASK_NAME,
                            GPIO_ISR_TASK_STACK_SIZE,
                            NULL,
                            GPIO_ISR_TASK_PRIORITY,
                            User_NewTask(),
                            0);

    clockbell_level = gpio_get_level(GPIO_NUM_35);
    return;
}

/**
 * @brief  读取停止响铃的按钮状态
 * @note    闹铃开启时默认高电平
 *
 * @returns
 *  -   0(low) or 1(high)
 *
 */
int read_user_gpio(void)
{
    return (int)clockbell_level;
}

/**
 * @brief  设置响铃的IO状态
 *
 * @param  level 0(low), 1(high)
 *
 */
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TICK_VERSION)
static uint32_t BellIOControl = true;
/// @brief 在TICK模式下对 BELL_IO 控制开关
/// @param
/// @note 只有两种状态, 每使用一次, 状态翻转
ISR_SAFE uint32_t BellControlOFFON(void)
{
    return BellIOControl = !BellIOControl;
}
ISR_SAFE
#endif
esp_err_t set_user_bell(const int level)
{
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TICK_VERSION)
    if(BellIOControl == false)
    {
        // 暂停对 BELL_IO 的控制
        gpio_set_level(GPIO_NUM_32, false);
        return ESP_ERR_NOT_SUPPORTED;
    }
        
#endif
    return gpio_set_level(GPIO_NUM_32, (level) ? true : false);
}

/**
 * @brief  显示四位数码管记录的状态
 * @note 没屌用
 *
 */
void show_LED_status(void)
{
    ESP_LOGI(TAG, "high |(DP) G F E D C B A| low");
    ESP_LOGI(TAG, "digit 1 LED_status: %02x", My_LED_status[1]);
    ESP_LOGI(TAG, "digit 2 LED_status: %02x", My_LED_status[2]);
    ESP_LOGI(TAG, "digit 3 LED_status: %02x", My_LED_status[3]);
    ESP_LOGI(TAG, "digit 4 LED_status: %02x", My_LED_status[4]);
}

/// @brief 关闭所有LED
/// @param
#if (LED_DRIVER_VERSION_MAJOR != LED_TASK_VERSION)
ISR_SAFE
#endif
void User_LEDall_off(void)
{

    // 关闭所有LED
    User_LED_on(1, 'A');
    User_LED_on(1, 'B');
    User_LED_on(1, 'C');
    User_LED_on(1, 'D');
    User_LED_on(1, 'E');
    User_LED_on(1, 'F');
    User_LED_on(1, 'G');
    User_LED_on(1, 'H');

    // 关闭所有digital IO
    User_LED_digital_on(0, 0);
    User_LED_digital_on(0, 1);
    User_LED_digital_on(0, 2);
    User_LED_digital_on(0, 3);
    User_LED_digital_on(0, 4);
}

#if (LED_DRIVER_VERSION_MAJOR != LED_TASK_VERSION)
ISR_SAFE
#endif
void User_LEDshow_num(const int num, const unsigned int digit_num)
{
    if (digit_num > 4)
    {
        ESP_LOGW(TAG, "the digit_num:%u is not a digital tube", digit_num);
        return;
    }

    User_LEDall_off();
    User_LED_digital_on(1, digit_num);
    if(digit_num == 0)
        (num) ? User_LED_digital_on(1, digit_num) : User_LED_digital_on(0, digit_num);
    switch (num)
    {
    case 0:
        User_LED_on(0, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(0, 'D');
        User_LED_on(0, 'E');
        User_LED_on(0, 'F');
        User_LED_on(1, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b00111111;
        break;
    case 1:
        User_LED_on(1, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(1, 'D');
        User_LED_on(1, 'E');
        User_LED_on(1, 'F');
        User_LED_on(1, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b00000110;
        break;
    case 2:
        User_LED_on(0, 'A');
        User_LED_on(0, 'B');
        User_LED_on(1, 'C');
        User_LED_on(0, 'D');
        User_LED_on(0, 'E');
        User_LED_on(1, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01011011;
        break;
    case 3:
        User_LED_on(0, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(0, 'D');
        User_LED_on(1, 'E');
        User_LED_on(1, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01001111;
        break;
    case 4:
        User_LED_on(1, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(1, 'D');
        User_LED_on(1, 'E');
        User_LED_on(0, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01100110;
        break;
    case 5:
        User_LED_on(0, 'A');
        User_LED_on(1, 'B');
        User_LED_on(0, 'C');
        User_LED_on(0, 'D');
        User_LED_on(1, 'E');
        User_LED_on(0, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01101101;
        break;
    case 6:
        User_LED_on(0, 'A');
        User_LED_on(1, 'B');
        User_LED_on(0, 'C');
        User_LED_on(0, 'D');
        User_LED_on(0, 'E');
        User_LED_on(0, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01111101;
        break;
    case 7:
        User_LED_on(0, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(1, 'D');
        User_LED_on(1, 'E');
        User_LED_on(1, 'F');
        User_LED_on(1, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b00000111;
        break;
    case 8:
        User_LED_on(0, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(0, 'D');
        User_LED_on(0, 'E');
        User_LED_on(0, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01111111;
        break;
    case 9:
        User_LED_on(0, 'A');
        User_LED_on(0, 'B');
        User_LED_on(0, 'C');
        User_LED_on(0, 'D');
        User_LED_on(1, 'E');
        User_LED_on(0, 'F');
        User_LED_on(0, 'G');
        User_LED_on(1, 'H');
        My_LED_status[digit_num] = 0b01101111;
        break;
    default:
        ESP_LOGE(TAG, "User_LEDshow_num: num error");
        break;
    }
}
