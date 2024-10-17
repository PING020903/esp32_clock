#ifndef __KEEP_STAMP_H__
#define __KEEP_STAMP_H__

#define KEEP_TIMESTAMP_SOFTWARE_VERSION 0
#define KEEP_TIMESTAMP_HARDWARE_VERSION 1


#if KEEP_TIMESTAMP_HARDWARE_VERSION
time_t get_User_timestamp2(void);
time_t *Set_User_timestamp(void);
void keep_userTimestamp_timer_ISR(void);
#endif
#endif