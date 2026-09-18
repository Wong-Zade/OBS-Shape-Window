#include <obs-module.h>
#include <graphics/graphics.h>
#include <util/dstr.h>
#include <string.h>

struct shape_window {
    obs_source_t *context;
    obs_source_t *child;
    char *child_name;
    int shape;
    double radius;
    gs_texrender_t *texrender;
    gs_effect_t *effect;
    gs_eparam_t *image;
    gs_eparam_t *size;
    gs_eparam_t *radius_param;
    gs_eparam_t *shape_param;
};

static const char *shape_window_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("ShapeWindow");
}

static void destroy_graphics(struct shape_window *s)
{
    if (s->effect) {
        gs_effect_destroy(s->effect);
        s->effect = NULL;
        s->image = NULL;
        s->size = NULL;
        s->radius_param = NULL;
        s->shape_param = NULL;
    }
    if (s->texrender) {
        gs_texrender_destroy(s->texrender);
        s->texrender = NULL;
    }
}

static void release_child(struct shape_window *s)
{
    if (s->child) {
        if (s->context)
            obs_source_remove_active_child(s->context, s->child);
        obs_source_release(s->child);
        s->child = NULL;
    }
}

static void set_child(struct shape_window *s, const char *name)
{
    if (!name)
        name = "";

    if (s->child_name && strcmp(s->child_name, name) == 0)
        return;

    release_child(s);
    bfree(s->child_name);
    s->child_name = bstrdup(name);

    if (*name) {
        obs_source_t *child = obs_get_source_by_name(name);
        if (child && child != s->context &&
            strcmp(obs_source_get_unversioned_id(child), "obs_shape_window") != 0) {
            s->child = child;
            if (s->context)
                obs_source_add_active_child(s->context, s->child);
        } else if (child) {
            obs_source_release(child);
        }
    }
}

static void *shape_window_create(obs_data_t *settings, obs_source_t *source)
{
    struct shape_window *s = bzalloc(sizeof(*s));
    s->context = source;
    s->shape = 0;
    s->radius = 40.0;
    set_child(s, obs_data_get_string(settings, "source"));
    s->shape = (int)obs_data_get_int(settings, "shape");
    s->radius = obs_data_get_double(settings, "radius");
    return s;
}

static void shape_window_destroy(void *data)
{
    struct shape_window *s = data;
    release_child(s);
    destroy_graphics(s);
    bfree(s->child_name);
    bfree(s);
}

static void shape_window_update(void *data, obs_data_t *settings)
{
    struct shape_window *s = data;
    set_child(s, obs_data_get_string(settings, "source"));
    s->shape = (int)obs_data_get_int(settings, "shape");
    s->radius = obs_data_get_double(settings, "radius");
}

static uint32_t shape_window_width(void *data)
{
    struct shape_window *s = data;
    return s->child ? obs_source_get_width(s->child) : 0;
}

static uint32_t shape_window_height(void *data)
{
    struct shape_window *s = data;
    return s->child ? obs_source_get_height(s->child) : 0;
}

static void shape_window_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(seconds);
}

static bool source_enum_cb(void *param, obs_source_t *src)
{
    obs_property_t *property = param;
    const char *id = obs_source_get_unversioned_id(src);
    const uint32_t flags = obs_source_get_output_flags(src);

    if ((flags & OBS_SOURCE_VIDEO) && strcmp(id, "obs_shape_window") != 0) {
        const char *name = obs_source_get_name(src);
        obs_property_list_add_string(property, name, name);
    }
    return true;
}

static void shape_window_enum_active(void *data, obs_source_enum_proc_t cb, void *param)
{
    struct shape_window *s = data;
    if (s->child && cb)
        cb(s->context, s->child, param);
}

static obs_properties_t *shape_window_properties(void *data)
{
    UNUSED_PARAMETER(data);

    obs_properties_t *props = obs_properties_create();

    obs_property_t *source = obs_properties_add_list(
        props, "source", obs_module_text("Source"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(source, "", "");
    obs_enum_sources(source_enum_cb, source);

    obs_property_t *shape = obs_properties_add_list(
        props, "shape", obs_module_text("Shape"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(shape, obs_module_text("Circle"), 0);
    obs_property_list_add_int(shape, obs_module_text("RoundedRectangle"), 1);

    obs_properties_add_float_slider(
        props, "radius", obs_module_text("Radius"), 0.0, 2000.0, 1.0);

    return props;
}

static void shape_window_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "shape", 0);
    obs_data_set_default_double(settings, "radius", 40.0);
}

static bool ensure_graphics(struct shape_window *s)
{
    if (!s->effect) {
        char *path = obs_module_file("shape-window.effect");
        s->effect = gs_effect_create_from_file(path, NULL);
        bfree(path);
        if (!s->effect)
            return false;

        s->image = gs_effect_get_param_by_name(s->effect, "image");
        s->size = gs_effect_get_param_by_name(s->effect, "uv_size");
        s->radius_param = gs_effect_get_param_by_name(s->effect, "radius");
        s->shape_param = gs_effect_get_param_by_name(s->effect, "shape_mode");
    }

    if (!s->texrender)
        s->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

    return s->texrender != NULL;
}

static void shape_window_render(void *data, gs_effect_t *unused_effect)
{
    struct shape_window *s = data;
    UNUSED_PARAMETER(unused_effect);

    if (!s->child)
        return;

    const uint32_t cx = obs_source_get_width(s->child);
    const uint32_t cy = obs_source_get_height(s->child);
    if (!cx || !cy || !ensure_graphics(s))
        return;

    const enum gs_color_space space = obs_source_get_color_space(s->child, 0, NULL);

    gs_texrender_reset(s->texrender);
    if (!gs_texrender_begin_with_color_space(s->texrender, cx, cy, space))
        return;

    struct vec4 clear = {0, 0, 0, 0};
    gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
    gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
    obs_source_video_render(s->child);
    gs_texrender_end(s->texrender);

    gs_texture_t *texture = gs_texrender_get_texture(s->texrender);
    if (!texture)
        return;

    struct vec2 size = {(float)cx, (float)cy};
    gs_effect_set_texture_srgb(s->image, texture);
    gs_effect_set_vec2(s->size, &size);
    gs_effect_set_float(s->radius_param, (float)s->radius);
    gs_effect_set_float(s->shape_param, (float)s->shape);

    while (gs_effect_loop(s->effect, "Draw"))
        gs_draw_sprite(NULL, 0, cx, cy);
}

struct obs_source_info shape_window_source_info = {
    .id = "obs_shape_window",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE,
    .get_name = shape_window_get_name,
    .create = shape_window_create,
    .destroy = shape_window_destroy,
    .get_width = shape_window_width,
    .get_height = shape_window_height,
    .get_defaults = shape_window_defaults,
    .get_properties = shape_window_properties,
    .update = shape_window_update,
    .video_tick = shape_window_tick,
    .video_render = shape_window_render,
    .enum_active_sources = shape_window_enum_active,
};
