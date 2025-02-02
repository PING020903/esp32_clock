#ifndef _USER_IO_INIT_H_
#define _USER_IO_INIT_H_

#include "User_main.h"


void User_gpio_init(void);

int read_user_gpio(void);

#if (BELL_DRIVER_VERSION_MAJOR == BELL_TICK_VERSION)
uint32_t BellControlOFFON(void);
#endif

esp_err_t set_user_bell(const int level);

void show_LED_status(void);

void User_LEDall_off(void);

/// @brief 显示数字
/// @param num 需要被显示的数字
/// @param digit_num 需要被显示数码管的编号
/// @note 只打开了digital IO, 并未做关闭处理
void User_LEDshow_num(const int num, const unsigned int digit_num);
#endif