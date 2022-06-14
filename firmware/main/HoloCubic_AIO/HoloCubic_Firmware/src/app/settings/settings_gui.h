#ifndef APP_EXAMPLE_GUI_H
#define APP_EXAMPLE_GUI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define ANIEND                      \
    while (lv_anim_count_running()) \
        lv_task_handler(); //等待动画完成

void settings_gui_init(void);
void display_settings(const char *cur_ver, const char *new_ver, lv_scr_load_anim_t anim_type);
void settings_gui_del(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif