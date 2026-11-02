/*
    MIT License
    
    Copyright (c) 2026 Snow Pierce
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

// .JSON to .H Tiled Level Converter (For GameTank)

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <string.h>
#include "third_party/cJSON/cJSON.h"

typedef struct {
    char *name;
    int width;
    int height;
    int *data;
}
TileLayer;

typedef struct {
    int width;
    int height;
    int *data;
}
MapLayer;

struct MapLevel {
    short map_width;
    short map_height;
    short tile_pixel_width;
    short tile_pixel_height;
    short map_is_infinite;
};

static FILE *log_file;

static void log_message(const char *message) {
    fprintf(log_file ? log_file : stderr, "%s\n", message);
}

static cJSON *read_json_file(const char *path) {

    FILE * file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        perror("file size");
        fclose(file);
        return NULL;
    }

    char *text = malloc((size_t) size + 1);
    if (text == NULL) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(text, 1, (size_t) size, file);
    fclose(file);

    if (bytes_read != (size_t) size) {
        fprintf(log_file ? log_file : stderr, "Could not read %s\n", path);
        free(text);
        return NULL;
    }

    text[bytes_read] = '\0';

    cJSON *json = cJSON_ParseWithLength(text, bytes_read);
    if (json == NULL) {
        const char * error_ptr = cJSON_GetErrorPtr();
        fprintf(log_file ? log_file : stderr,
            "JSON parse error in %s near: %.40s\n",
            path, error_ptr ? error_ptr : "(unknown)");
    }

    free(text);
    return json;
}

static void free_map_layers(MapLayer *layers, size_t count) {
    if (layers == NULL) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        free(layers[i].data);
    }
    free(layers);
}

static int parse_layer(const cJSON *layer_json, MapLayer *layer) {
    const cJSON * data_json = cJSON_GetObjectItemCaseSensitive(layer_json, "data");
    const cJSON * width_json = cJSON_GetObjectItemCaseSensitive(layer_json, "width");
    const cJSON * height_json = cJSON_GetObjectItemCaseSensitive(layer_json, "height");

    if (!cJSON_IsObject(layer_json) || !cJSON_IsArray(data_json) || !cJSON_IsNumber(width_json) || !cJSON_IsNumber(height_json)) {
        log_message("Missing or invalid layer fields");
        return 0;
    }

    double width_value = cJSON_GetNumberValue(width_json);
    double height_value = cJSON_GetNumberValue(height_json);

    if (width_value <= 0 || height_value <= 0 || width_value > INT_MAX || height_value > INT_MAX ||
        width_value != (int) width_value || height_value != (int) height_value) 
    {
            log_message("Invalid layer dimensions");
            return 0;
    }

    layer->width = (int)width_value;
    layer->height = (int)height_value;

    size_t width = (size_t)layer->width;
    size_t height = (size_t)layer->height;

    if (height > SIZE_MAX / width) {
        log_message("Layer dimensions are too large");
        return 0;
    }

    size_t count = width * height;
    if (count != (size_t) cJSON_GetArraySize(data_json) || count > SIZE_MAX / sizeof * layer -> data) {
        log_message("Layer data size does not match its dimensions");
        return 0;
    }

    layer->data = malloc(count * sizeof * layer -> data);
    if (layer -> data == NULL) {
        perror("allocating layer data");
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        const cJSON * item = cJSON_GetArrayItem(data_json, (int) i);

        if (!cJSON_IsNumber(item)) {
            fprintf(log_file ? log_file : stderr, "Non-numeric tile at index %zu\n", i);
            free(layer -> data);
            layer->data = NULL;
            return 0;
        }

        double value = cJSON_GetNumberValue(item);
        if (value < INT_MIN || value > INT_MAX || value != (int) value) {
            fprintf(log_file ? log_file : stderr,"Tile value out of range at index %zu\n", i);
            free(layer->data);
            layer->data = NULL;
            return 0;
        }

        layer->data[i] = (int)value;
    }

    return 1;
}

static int parse_layers(const cJSON *json_file, MapLayer **out_layers, size_t *out_count) {
    *out_layers = NULL;
    *out_count = 0;

    const cJSON * layers_json = cJSON_GetObjectItemCaseSensitive(json_file, "layers");

    if (!cJSON_IsArray(layers_json)) {
        log_message("Missing or invalid layers array");
        return 0;
    }

    int json_count = cJSON_GetArraySize(layers_json);
    if (json_count <= 0) {
        log_message("Map has no layers");
        return 0;
    }

    size_t count = (size_t) json_count;
    MapLayer *layers = calloc(count, sizeof * layers);

    if (layers == NULL) {
        perror("allocating layers");
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        const cJSON * layer_json = cJSON_GetArrayItem(layers_json, (int) i);

        if (!parse_layer(layer_json, & layers[i])) {
            free_map_layers(layers, count);
            return 0;
        }
    }

    *out_layers = layers;
    *out_count = count;
    return 1;
}

static int parse_json_file(const cJSON *json, struct MapLevel *level, MapLayer **layers, size_t *layer_count) {

    if (!cJSON_IsObject(json) || level == NULL || layers == NULL || layer_count == NULL) {
        log_message("Invalid JSON root or output pointer");
        return 0;
    }

    *level = (struct MapLevel) {
        0
    };

    *layers = NULL;
    *layer_count = 0;

    const cJSON *map_height = cJSON_GetObjectItem(json, "height");
    const cJSON *map_width = cJSON_GetObjectItem(json, "width");

    const cJSON *tile_pixel_width = cJSON_GetObjectItem(json, "tilewidth");;
    const cJSON *tile_pixel_height = cJSON_GetObjectItem(json, "tileheight");

    const cJSON * infinite = cJSON_GetObjectItemCaseSensitive(json, "infinite");

    
    if(cJSON_IsNumber(map_height)){
        level->map_height = map_height->valueint;
    }

    if(cJSON_IsNumber(map_width)){
        level->map_width = map_width->valueint;
    }


    if(cJSON_IsNumber(tile_pixel_height)){
        level->tile_pixel_height = tile_pixel_height->valueint;
    }

    if(cJSON_IsNumber(tile_pixel_width)){
        level->tile_pixel_width = tile_pixel_width->valueint;
    }

    if (!cJSON_IsBool(infinite)) {
        log_message("Missing or invalid infinite field");
        return 0;
    }

    level->map_is_infinite = cJSON_IsTrue(infinite) ? 1 : 0;

    return parse_layers(json, layers, layer_count);
}

static char *create_file_name(const char *current_file_name)
{
    if (current_file_name == NULL) {
        return NULL;
    }

    const char *filename = strrchr(current_file_name, '/');
    filename = filename ? filename + 1 : current_file_name;

    const char *windows_separator = strrchr(filename, '\\');
    if (windows_separator != NULL) {
        filename = windows_separator + 1;
    }

    size_t length = strlen(filename);
    char *buffer = malloc(length + 1);
    if (buffer == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < length; ++i) {
        unsigned char character = (unsigned char)filename[i];

        if (character == ' ') {
            buffer[i] = '_';
        } else {
            buffer[i] = (char)toupper(character);
        }
    }

    buffer[length] = '\0';
    return buffer;
}

static int create_header_file(const char* file_name, const struct MapLevel *level, const MapLayer *layers, size_t layer_count, int index) {
    
    char filename[64];

    int length = snprintf(filename, sizeof filename,"%s_%d.h", file_name, index);
    if (length < 0 || (size_t) length >= sizeof filename) {
        log_message("Output filename is too long");
        return 0;
    }

    FILE * file = fopen(filename, "w");
    if (file == NULL) {
        perror(filename);
        return 0;
    }

    int ok = fprintf(file,
        "#ifndef GENERATED_HEADER_%d_H\n"
        "#define GENERATED_HEADER_%d_H\n\n"
        "#include <stddef.h>\n\n"
        "struct MapLayer {\n"
        "    int width;\n"
        "    int height;\n"
        "    const int *data;\n"
        "};\n\n"
        "#define MAP_HEIGHT %d\n"
        "#define MAP_WIDTH %d\n"
        "#define TILE_PIXEL_HEIGHT %d\n"
        "#define TILE_PIXEL_WIDTH %d\n"
        "#define MAP_IS_INFINITE %d\n"
        "#define MAP_LAYER_COUNT %zu\n\n",
        index, index,
        level->map_height, level->map_width,
        level->tile_pixel_height, level->tile_pixel_width,
        level->map_is_infinite,
        layer_count) >= 0;

    for (size_t i = 0; ok && i < layer_count; ++i) {
        size_t tile_count = (size_t)layers[i].width * (size_t)layers[i].height;

        if (fprintf(file, "static const int layer_%zu_data[] = {\n", i) < 0) {
            ok = 0;
            break;
        }

        for (size_t j = 0; ok && j < tile_count; ++j) {
            if (fprintf(file, "%d%s%s", layers[i].data[j], j + 1 < tile_count ? "," : "", (j + 1) % 12 == 0 || j + 1 == tile_count ? "\n" : " ") < 0) {
                ok = 0;
            }
        }

        if (ok && fprintf(file, "};\n\n") < 0) {
            ok = 0;
        }
    }

    if (ok && fprintf(file,"static const struct MapLayer map_layers[] = {\n") < 0) {
        ok = 0;
    }

    for (size_t i = 0; ok && i < layer_count; ++i) {
        if (fprintf(file,
                "{ %d, %d, layer_%zu_data }%s\n",
                layers[i].width,
                layers[i].height,
                i,
                i + 1 < layer_count ? "," : "") < 0) {
            ok = 0;
        }
    }

    if (ok && fprintf(file, "};\n\n#endif\n") < 0) {
        ok = 0;
    }

    if (fclose(file) != 0) {
        perror("closing generated header");
        return 0;
    }

    if (!ok) {
        fprintf(log_file ? log_file : stderr,"Error writing to %s\n", filename);
        return 0;
    }

    fprintf(log_file ? log_file : stderr, "Wrote %s\n", filename);
    return 1;
}

int main(int argc, char *argv[]) {

    log_file = fopen("log.txt", "a");

    if (log_file == NULL) {
        perror("log.txt");
    }

    if (argc < 2) {
        log_message("No input files provided.");
        if (log_file != NULL) {
            fclose(log_file);
        }
        return 1;
    }

    int exit_status = 0;

    for (int i = 1; i < argc; ++i) {
        cJSON * json = read_json_file(argv[i]);
        if (json == NULL) {
            exit_status = 1;
            continue;
        }

        struct MapLevel level;
        MapLayer *layers = NULL;
        size_t layer_count = 0;
        const char* file_name = "";

        int parse_result = parse_json_file(json, &level, &layers, &layer_count);
        if (!parse_result) {
            fprintf(log_file ? log_file : stderr, "Could not parse map data from %s\n", argv[i]);
            exit_status = 1;
        } else {

            file_name = create_file_name(argv[i]);

            int header_result = create_header_file("MAP_GENERATED", &level, layers, layer_count, i);
            
            if (!header_result) {
                exit_status = 1;
            }
        }

        free_map_layers(layers, layer_count);
        cJSON_Delete(json);
    }

    if (log_file != NULL) {
        fclose(log_file);
    }

    return exit_status;
}
