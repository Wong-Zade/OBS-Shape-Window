#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-shape-window", "en-US")

extern struct obs_source_info shape_window_source_info;

bool obs_module_load(void)
{
    obs_register_source(&shape_window_source_info);
    blog(LOG_INFO, "[OBS Shape Window] loaded");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[OBS Shape Window] unloaded");
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Displays another OBS source through a circle or rounded rectangle.";
}
