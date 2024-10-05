#pragma once

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
    size_t current;
} image;

typedef struct {
    image* img;
    int current_x;
    int current_y;
} raster_data;

char ascii_by_brightness[] = "`.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$Bg0MNWQ%&@";

static int clamp(int val, int min, int max) {
	if(val < min) {
        return min;
	}
    if(val > max) {
        return max;
    }
    return val;
}
static int clamp_min(int val, int min) {
	if(val < min) {
        return min;
	}
    return val;
}
static int clamp_max(int val, int max) {
    if(val > max) {
        return max;
    }
    return val;
}

static void get_rgba(int pixel, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a) {
    *r = (char)(pixel>>24);
    *g = (char)(pixel>>16);
    *b = (char)(pixel>>8);
    *a = (char)pixel;
}

static void set_current_pixel(image* img, int x, int y) {
    img->current = y*img->width*img->channels + x*img->channels;
}

static int get_pixel(image* img) {
    assert(img->channels <= 4, "Cant convert image with more than 4 channels");
    int pixel = 0;
    for(int i = 0; i < img->channels; i++) {
        pixel += ((int)*(img->data+img->current+i)) << (8*i);
    }
    if(img->channels < 4) {
        pixel += 255; //make opaque if no alpha channel
    }
    img->current += img->channels;
    return pixel;
}

static void read_pixels(image* img, int* buf, int x, int y, int count) {
    set_current_pixel(img, x, y);
    for(int i=0; i < count; i++) {
        buf[i] = get_pixel(img);
    }
}
static void read_pixels2d(image* img, int** buf, int x, int y, int count_x, int count_y) {
    for(int j=0; j < count_y; j++) {
        read_pixels(img, buf[j], x, y+j, count_x);
    }
}

float get_brightness(int pixel, int channels) {
    unsigned char r, g, b, a;
    get_rgba(pixel, &r, &g, &b, &a);
    const float inv_255 = 1.f/255.f;
    float brightness = 1.f-(r*inv_255 + g*inv_255 + b*inv_255)/clamp(channels, 1, 3) * (a*inv_255);
    return brightness;
}

// res == 1
static char get_char(raster_data* rd) {
    rd->current_x = (rd->current_x+1)%rd->img->width;
    float brightness = get_brightness(get_pixel(rd->img), rd->img->channels);
    int ascii_count = sizeof(ascii_by_brightness)/sizeof(ascii_by_brightness[0]) - 1;
	return ascii_by_brightness[(int)(brightness*(ascii_count-1))];
}

// res > 1
static char get_char_w(raster_data* rd, int** buf, int res) {
    int dist_to_right = rd->img->width - (rd->current_x+res);
    int dist_to_bottom = rd->img->height - (rd->current_y+res);
    int count_x = res - min(dist_to_right, 0);
    int count_y = res - min(dist_to_bottom, 0);
    read_pixels2d(rd->img, buf, rd->current_x, rd->current_y, count_x, count_y);
    if(dist_to_right > 0) {
        count_y = 0;
    }
    rd->current_y = (rd->current_y+count_y)%rd->img->height;
    rd->current_x = (rd->current_x+count_x)%rd->img->width;

    float brightness = 0;

    for(int j=0;j<res;j++) {
        for(int i=0; i<res; i++) {
            brightness += get_brightness(buf[j][i], rd->img->channels);
        }
    }

    brightness /= res*res;
    int ascii_count = sizeof(ascii_by_brightness)/sizeof(ascii_by_brightness[0]) - 1;
    return ascii_by_brightness[(int)(brightness*(ascii_count-1))];
}


static void write_raster_to_file(image* img, FILE* file, int res) {
    raster_data rd = {img, 0, 0};
    char* line_buf;
	if(res == 1) { 
        line_buf = malloc((img->width + 1) * sizeof(char));
        line_buf[img->width] = '\0';
        for(int j=0;j<img->height;j+=1) {
            for(int i=0;i<img->width;i+=1) {
                line_buf[i] = get_char(&rd);
            }
            fprintf(file, "%s\n", line_buf);
        }
	}else {
        int** buf = malloc(sizeof(int*) * res);
        for(int i=0;i<res;i++) {
            buf[i] = malloc(sizeof(int) * res);
        }
        int x_len = (img->width-1)/res + 1;
        int y_len = (img->height-1)/res + 1;
        int line_buf_len = x_len;
        line_buf = malloc((line_buf_len + 1) * sizeof(char));
        line_buf[line_buf_len] = '\0';
        for(int j=0;j<y_len;j+=1) {
            for(int i=0;i<x_len;i+=1) {
                line_buf[i] = get_char_w(&rd, buf, res);
            }
            fprintf(file, "%s\n", line_buf);
        }
        for(int i=0;i<res;i++) {
            free(buf[i]);
        }
        free(buf);
	}
    free(line_buf);
    
}

int raster_to_ascii(char* image_name, char* file_out_name, int resolution) {
    assert(resolution >= 1, "resolution can't be lower than 1");
    int width, height, channels;
    unsigned char *stb_img = stbi_load(image_name, &width, &height, &channels, 0);
    if (stb_img == NULL) {
        printf("Failed to load image\n");
        return -1;
    }

    if(strcmp(file_out_name, "") == 0) {
        int img_name_len = strlen(image_name);
        const char name_postfix[] = ".out.txt";
        int name_postfix_len = strlen(name_postfix);
        file_out_name = malloc(sizeof(char) * (img_name_len + name_postfix_len + 1));
        strcpy_s(file_out_name, img_name_len + 1, image_name);
        strcpy_s(file_out_name + img_name_len, name_postfix_len + 1, name_postfix);
    }
    FILE* file_out = fopen(file_out_name, "w");
    free(file_out_name);

    image img = {stb_img, width, height, channels, 0};

    write_raster_to_file(&img, file_out, resolution);

    fclose(file_out);
    stbi_image_free(stb_img);
    return 0;
}