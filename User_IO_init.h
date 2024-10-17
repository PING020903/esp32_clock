#ifndef _USER_IO_INIT_H_
#define _USER_IO_INIT_H_


void User_LED_on(const bool level, const int IO_num);

void User_LED_digital_on(const bool level, const int IO_num);

void User_gpio_init(void);

int read_user_gpio(void);

void set_user_bell(const int level);

void show_LED_status(void);

void User_LEDall_off(void);

void User_LEDshow_num(const int num, const unsigned int digit_num);
#endif