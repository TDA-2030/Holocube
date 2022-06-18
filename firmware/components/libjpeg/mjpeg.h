#ifndef __MJPEG_H
#define __MJPEG_H 

#include "stdint.h"

#ifdef __cplusplus 
extern "C" {
#endif


void mjpegdraw(const uint8_t *mjpegbuffer, uint32_t size, uint8_t *outbuffer, void (*draw)(int x, int y, int w, int h, uint16_t *data));

#ifdef __cplusplus 
}
#endif

#endif

