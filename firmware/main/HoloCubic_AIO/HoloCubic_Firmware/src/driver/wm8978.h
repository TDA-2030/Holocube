#ifndef __WM8978_H
#define __WM8978_H

#include <stdio.h>

#define EQ1_80Hz      0X00
#define EQ1_105Hz     0X01
#define EQ1_135Hz     0X02
#define EQ1_175Hz     0X03

#define EQ2_230Hz     0X00
#define EQ2_300Hz     0X01
#define EQ2_385Hz     0X02
#define EQ2_500Hz     0X03

#define EQ3_650Hz     0X00
#define EQ3_850Hz     0X01
#define EQ3_1100Hz    0X02
#define EQ3_14000Hz   0X03

#define EQ4_1800Hz    0X00
#define EQ4_2400Hz    0X01
#define EQ4_3200Hz    0X02
#define EQ4_4100Hz    0X03

#define EQ5_5300Hz    0X00
#define EQ5_6900Hz    0X01
#define EQ5_9000Hz    0X02
#define EQ5_11700Hz   0X03



bool WM8978_begin(void);
void WM8978_cfgADDA(uint8_t dacen, uint8_t adcen);
void WM8978_cfgInput(uint8_t micen, uint8_t lineinen, uint8_t auxen);
void WM8978_cfgOutput(uint8_t dacen, uint8_t bpsen);
void WM8978_cfgI2S(uint8_t fmt, uint8_t len);
void WM8978_setMICgain(uint8_t gain);
void WM8978_setLINEINgain(uint8_t gain);
void WM8978_setAUXgain(uint8_t gain);
void WM8978_setHPvol(uint8_t voll, uint8_t volr);
void WM8978_setSPKvol(uint8_t volx);
void WM8978_set3D(uint8_t depth);
void WM8978_set3Ddir(uint8_t dir);
void WM8978_setEQ1(uint8_t cfreq, uint8_t gain);
void WM8978_setEQ2(uint8_t cfreq, uint8_t gain);
void WM8978_setEQ3(uint8_t cfreq, uint8_t gain);
void WM8978_setEQ4(uint8_t cfreq, uint8_t gain);
void WM8978_setEQ5(uint8_t cfreq, uint8_t gain);
void WM8978_setNoise(uint8_t enable, uint8_t gain);
void WM8978_setALC(uint8_t enable, uint8_t maxgain, uint8_t mingain);
void WM8978_setHPF(uint8_t enable);



#endif
