#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_WINDOW_CMD "/usr/bin/xterm"
#define DEFAULT_CURSOR_PATH "~/hatDE/src/assets/cursor.svg"
#define DEFAULT_FONT_PATH "~/hatDE/src/assets/JetBrainsMonoNerdFont-Regular.ttf"
#define DEFAULT_BACKGROUND_MODE "color"
#define DEFAULT_BACKGROUND_IMAGE ""

static void trim(char *value) {
    if (value == NULL) {
        return;
    }

    char *start = value;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != value) {
        memmove(value, start, strlen(start) + 1);
    }

    size_t len = strlen(value);
    while (len > 0 && isspace((unsigned char)value[len - 1])) {
        value[len - 1] = '\0';
        len--;
    }
}

static void strip_quotes(char *value) {
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        value[len - 1] = '\0';
        memmove(value, value + 1, len - 1);
    }
}

static void expand_tilde(char *value, size_t size) {
    if (value == NULL) {
        return;
    }

    if (strncmp(value, "~/", 2) != 0) {
        return;
    }

    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return;
    }

    char expanded[512];
    snprintf(expanded, sizeof(expanded), "%s/%s", home, value + 2);
    strncpy(value, expanded, size - 1);
    value[size - 1] = '\0';
}

void config_set_defaults(Config *config) {
    if (config == NULL) {
        return;
    }

    snprintf(config->window_cmd, sizeof(config->window_cmd), "%s", DEFAULT_WINDOW_CMD);
    snprintf(config->cursor_path, sizeof(config->cursor_path), "%s", DEFAULT_CURSOR_PATH);
    snprintf(config->font_path, sizeof(config->font_path), "%s", DEFAULT_FONT_PATH);
    snprintf(config->background_mode, sizeof(config->background_mode), "%s", DEFAULT_BACKGROUND_MODE);
    snprintf(config->background_image, sizeof(config->background_image), "%s", DEFAULT_BACKGROUND_IMAGE);
}

static void config_set_value(Config *config, const char *key, char *value) {
    if (config == NULL || key == NULL || value == NULL) {
        return;
    }

    trim(value);
    strip_quotes(value);
    expand_tilde(value, 256);

    if (strcmp(key, "WINDOW") == 0) {
        snprintf(config->window_cmd, sizeof(config->window_cmd), "%s", value);
    } else if (strcmp(key, "CURSOR") == 0) {
        snprintf(config->cursor_path, sizeof(config->cursor_path), "%s", value);
    } else if (strcmp(key, "FONT") == 0) {
        snprintf(config->font_path, sizeof(config->font_path), "%s", value);
    } else if (strcmp(key, "BACKGROUND") == 0) {
        snprintf(config->background_mode, sizeof(config->background_mode), "%s", value);
    } else if (strcmp(key, "BIMAGE") == 0) {
        snprintf(config->background_image, sizeof(config->background_image), "%s", value);
    }
}

int config_load(Config *config, const char *path) {
    if (config == NULL || path == NULL) {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        trim(line);
        if (line[0] == '\0') {
            continue;
        }

        char *key = line;
        char *value = NULL;
        char *colon = strchr(line, ':');
        if (colon != NULL) {
            *colon = '\0';
            value = colon + 1;
        } else {
            char *space = line;
            while (*space != '\0' && !isspace((unsigned char)*space)) {
                space++;
            }
            if (*space != '\0') {
                *space = '\0';
                value = space + 1;
            }
        }

        if (value == NULL) {
            continue;
        }

        trim(key);
        trim(value);
        config_set_value(config, key, value);
    }

    fclose(file);
    return 1;
}

static int config_ensure_directories(const char *path) {
    char buffer[512];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(buffer)) {
        return 0;
    }

    snprintf(buffer, sizeof(buffer), "%s", path);
    char *last_slash = strrchr(buffer, '/');
    if (last_slash == NULL) {
        return 1;
    }
    *last_slash = '\0';

    for (char *cursor = buffer + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mkdir(buffer, 0700) != 0 && errno != EEXIST) {
                return 0;
            }
            *cursor = '/';
        }
    }

    if (mkdir(buffer, 0700) != 0 && errno != EEXIST) {
        return 0;
    }

    return 1;
}

int config_write_default(const Config *config, const char *path) {
    if (config == NULL || path == NULL) {
        return 0;
    }

    if (!config_ensure_directories(path)) {
        return 0;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    fprintf(file, "# This is a placeholder\n\n");
    fprintf(file, "WINDOW: \"%s\"\n", config->window_cmd);
    fprintf(file, "CURSOR: \"%s\"\n", config->cursor_path);
    fprintf(file, "FONT \"%s\"\n\n", config->font_path);
    fprintf(file, "# desktop\n");
    fprintf(file, "BACKGROUND: \"%s\" # color | gradient | image\n", config->background_mode);
    fprintf(file, "BIMAGE: \"%s\"\n", config->background_image);

    fclose(file);
    return 1;
}
