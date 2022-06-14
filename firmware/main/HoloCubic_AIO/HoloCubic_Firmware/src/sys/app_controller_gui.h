#ifndef APP_CONTROLLER_GUI_H
#define APP_CONTROLLER_GUI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

void app_control_gui_init(void);
void app_control_gui_release(void);
void app_control_display_scr(const void *src_img, const char * app_name,
                                lv_scr_load_anim_t anim_type, bool force);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif