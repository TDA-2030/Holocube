#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "display.h"
#include "network.h"
#include "common.h"


static lv_disp_drv_t disp_drv;
static lv_disp_buf_t disp_buf;

lv_indev_t *indev_encoder;
int32_t encoder_diff;
lv_indev_state_t encoder_state;

static TaskHandle_t g_lvgl_task_handle;
/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
static SemaphoreHandle_t g_guisemaphore;
static esp_lcd_panel_handle_t panel_handle = NULL;

// #define TFT_MISO -1
// #define TFT_MOSI 21
// #define TFT_SCLK 18
// #define TFT_CS 47
// #define TFT_DC 48
// #define TFT_RST -1

#define TFT_MISO -1
#define TFT_MOSI 48
#define TFT_SCLK 21
#define TFT_CS 47
#define TFT_DC 18
#define TFT_RST -1

/**
 * @brief Task to generate ticks for LVGL.
 *
 * @param pvParam Not used.
 */
static void lv_tick_inc_cb(void *data)
{
    uint32_t tick_inc_period_ms = (uint32_t) data;
    lv_tick_inc(tick_inc_period_ms);
}

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, &color_p->full);
    // ESP_LOGI(__FUNCTION__, "%s:%d (%d, %d, %d, %d)", __FILE__, __LINE__, area->x1, area->y1, area->x2 + 1, area->y2 + 1);

    // lv_disp_flush_ready(disp);
}

static void lvgl_task(void *pvParam)
{
    (void) pvParam;
    g_guisemaphore = xSemaphoreCreateMutex();

    do {
        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(g_guisemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(g_guisemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(10));

    } while (true);

    vTaskDelete(NULL);
}

void lvgl_acquire(void)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (g_lvgl_task_handle != task) {
        xSemaphoreTake(g_guisemaphore, portMAX_DELAY);
    }
}

void lvgl_release(void)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (g_lvgl_task_handle != task) {
        xSemaphoreGive(g_guisemaphore);
    }
}

static bool lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *user_data, void *event_data)
{
    (void) panel_io;
    (void) user_data;
    (void) event_data;

    if (disp_drv.flush_cb) {
        lv_disp_flush_ready(&disp_drv);
    }
    return false;
}


/* Will be called by the library to read the encoder */
static bool encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    data->enc_diff = encoder_diff;
    data->state = encoder_state;

    encoder_diff = 0;
    /*Return `false` because we are not buffering and no more data to read*/
    return false;
}

void lv_port_indev_init(void)
{
    /* Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    lv_indev_drv_t indev_drv;

    /*Register a encoder input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read;
    indev_encoder = lv_indev_drv_register(&indev_drv);

    /* Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     * add objects to the group with `lv_group_add_obj(group, obj)`
     * and assign this input device to group to navigate in it:
     * `lv_indev_set_group(indev_encoder, group);` */
}

static void LCD_ST7789_init(void)
{
    esp_err_t ret_val = ESP_OK;

    spi_bus_config_t buscfg = {0};
    buscfg.sclk_io_num = TFT_SCLK;
    buscfg.mosi_io_num = TFT_MOSI;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = 240 * 240 * sizeof(uint16_t);

    ret_val |= spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {0};
    io_config.dc_gpio_num = TFT_DC;
    io_config.cs_gpio_num = TFT_CS;
    io_config.pclk_hz = 40 * 1000000;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.user_ctx = NULL;
    io_config.on_color_trans_done = lcd_trans_done_cb,

    ret_val |= esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) SPI2_HOST, &io_config, &io_handle);

    esp_lcd_panel_dev_config_t panel_config = {0};
    panel_config.reset_gpio_num = TFT_RST;
    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.flags.reset_active_high = 0;

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle)); vTaskDelay(pdMS_TO_TICKS(300));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    esp_lcd_panel_disp_off(panel_handle, 0);
    esp_lcd_panel_invert_color(panel_handle, 1);
}

void lcd_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data)
{
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + w, y + h, data);
}

void Display::setRotation(uint8_t r)
{
    switch (r) {
    case 1:
        esp_lcd_panel_mirror(panel_handle, 0, 0);
        // esp_lcd_panel_swap_xy();
        break;
    case 2:
        esp_lcd_panel_mirror(panel_handle, 1, 0);
        break;
    case 3:
        esp_lcd_panel_mirror(panel_handle, 0, 1);
        esp_lcd_panel_set_gap(panel_handle, 0, 80);
        break;
    default: // case 0:
        esp_lcd_panel_mirror(panel_handle, 0, 0);
        break;
    }
}

void Display::fillScreen(uint16_t color)
{
    int w = _width;
    int h = _height;
    uint16_t *buffer = (uint16_t *)malloc(w * sizeof(uint16_t));
    if (NULL == buffer) {
        return;
    } else {
        for (size_t i = 0; i < w; i++) {
            buffer[i] = color;
        }

        for (int y = 0; y < h; y++) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, w, y + 1, buffer);
        }

        free(buffer);
    }
}

void Display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data)
{
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + w, y + h, data);
}

void Display::init(uint8_t rotation, uint8_t backLight)
{
    _width = 240;
    _height = 240;
    ledcSetup(LCD_BL_PWM_CHANNEL, 5000, 8);
    ledcAttachPin(LCD_BL_PIN, LCD_BL_PWM_CHANNEL);

    setBackLight(backLight / 100.0);  // 设置亮度
    LCD_ST7789_init();
    setRotation(rotation); /* mirror 修改反转，如果加上分光棱镜需要改为4镜像*/
    fillScreen(0xdd);

    lv_init();
    size_t disp_buf_height = 20;
    uint32_t buf_alloc_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    lv_color_t *p_disp_buf = (lv_color_t *)heap_caps_malloc(_width * disp_buf_height * sizeof(lv_color_t) * 2, buf_alloc_caps);
    lv_color_t *p_disp_buf1 = p_disp_buf;
    lv_color_t *p_disp_buf2 = p_disp_buf + _width * disp_buf_height;
    ESP_LOGI(__FUNCTION__, "Try allocate two %u * %u display buffer, size:%u Byte", _width, disp_buf_height, _width * disp_buf_height * sizeof(lv_color_t) * 2);
    if (NULL == p_disp_buf) {
        ESP_LOGE(__FUNCTION__, "No memory for LVGL display buffer");
        esp_system_abort("Memory allocation failed");
    }

    lv_disp_buf_init(&disp_buf, p_disp_buf1, p_disp_buf2, _width * disp_buf_height);
    /*Initialize the display*/
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = _width;
    disp_drv.ver_res = _height;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &disp_buf;
    // 开启 LV_COLOR_SCREEN_TRANSP 屏幕具有透明和不透明样式
    lv_disp_drv_register(&disp_drv);

    const uint32_t tick_inc_period_ms = 5;
    esp_timer_create_args_t periodic_timer_args = {0};
    periodic_timer_args.callback = lv_tick_inc_cb;
    periodic_timer_args.name = "lv_tick";     /* name is optional, but may help identify the timer when debugging */
    periodic_timer_args.arg = (void *)tick_inc_period_ms;
    periodic_timer_args.dispatch_method = ESP_TIMER_TASK;
    periodic_timer_args.skip_unhandled_events = true;

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

    /* The timer has been created but is not running yet. Start the timer now */
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, tick_inc_period_ms * 1000));

    // BaseType_t ret_val = xTaskCreatePinnedToCore(lvgl_task, "lvgl_Task", 6 * 1024, NULL, configMAX_PRIORITIES - 3, &g_lvgl_task_handle, 0);
    // ESP_ERROR_CHECK((pdPASS == ret_val) ? ESP_OK : ESP_FAIL);
}

void Display::routine(void)
{
    lv_task_handler();
}

void Display::setBackLight(float duty)
{
    duty = constrain(duty, 0, 1);
    duty = 1 - duty;
    ledcWrite(LCD_BL_PWM_CHANNEL, (int)(duty * 255));
}
