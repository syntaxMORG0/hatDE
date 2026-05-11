#pragma once

typedef struct Config {
    char window_cmd[256];
    char cursor_path[256];
    char font_path[256];
    char background_mode[32];
    char background_image[256];
} Config;

void config_set_defaults(Config *config);
int config_load(Config *config, const char *path);
