#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus

class Display {
public:
    void init(uint8_t rotation, uint8_t backLight);
    void routine();
    void setBackLight(float);

    int16_t width(void) const
    {
        return _width;
    }
    int16_t height(void) const
    {
        return _height;
    }
    void setRotation(uint8_t r);

    bool getSwapBytes()
    {
        return 0;
    }
    void setSwapBytes(bool swap) {}

    void fillScreen(uint16_t color);

    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data);
    void pushImage(int x, int y, int w, int h, uint8_t *&data)
    {
        pushImage((int)x, (int)y, (int)w, (int)h, (const uint16_t *)data);
    }
    void initDMA() {}
    void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t *buffer = nullptr)
    {
        pushImage(x, y, w, h, data);
    }

    void setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) {}
    virtual void writeColor(uint16_t color) {}

private:
    int _width;
    int _height;
};

#endif

void lv_port_indev_init(void);
void lvgl_acquire(void);
void lvgl_release(void);

#endif
