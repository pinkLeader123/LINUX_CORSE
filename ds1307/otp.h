#ifndef OTP_H
#define OTP_H

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define step 30 // time-step default value is 30 seconds

uint32_t totp(uint8_t* key, size_t klen, uint64_t time, int digits);

uint32_t hotp(uint8_t* key, size_t klen, uint64_t interval, int digits);

double my_floor(double x);

time_t getTime(time_t T0);

#endif // !OTP