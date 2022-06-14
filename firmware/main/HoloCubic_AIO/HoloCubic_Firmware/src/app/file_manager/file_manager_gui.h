#ifndef APP_FILE_MANAGER_GUI_H
#define APP_FILE_MANAGER_GUI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define ANIEND                      \
    while (lv_anim_count_running()) \
        lv_task_handler(); //等待动画完成

void file_maneger_gui_init(void);
void display_file_manager_init(void);
void display_file_manager(const char *title, const char *domain,
                          const char *info, const char *ap_ip,
                          lv_scr_load_anim_t anim_type);
void file_manager_gui_del(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif