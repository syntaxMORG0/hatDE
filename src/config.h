#pragma once

typedef struct Config {
    char window_cmd[256];
    char cursor_path[256];
    char font_path[256];
    char background_mode[32];
    char background_image[256];
    char background_color[32];
    char title_bg[32];
    char title_fg[32];
    int title_height;
    int window_width;
    int window_height;
} Config;

void config_set_defaults(Config *config);
int config_load(Config *config, const char *path);
int config_write_default(const Config *config, const char *path);
