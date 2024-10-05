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
    img->current = y*img->width + x;
}

static void get_current_pixel_xy(image* img, int* x, int* y) {
    *y = img->current / img->width;
    *x = img->current - (*y) * img->width;
}

static int get_pixel_advance(image* img) {
    int pixel = 0;
    for(int i = 0; i < img->channels; i++) {
        pixel += ((int)*(img->data+(img->current+i)*img->channels)) << (8*i);
    }
    if(img->channels < 4) {
        pixel += 255; //make opaque if no alpha channel
    }
    img->current++;
    return pixel;
}

static void read_pixels(image* img, int* buf, int count) {
    for(int i=0; i < count; i++) {
        buf[i] = get_pixel_advance(img);
    }
}
static void read_pixels2d(image* img, int** buf, int count_x, int count_y) {
    size_t current = img->current;
    int x, y;
    get_current_pixel_xy(img, &x, &y);
    for(int j=0; j < count_y; j++) {
        img->current = current + img->width * j; //add row
        read_pixels(img, buf[j], count_x);
    }
    if(x + count_x < img->width) {
        img->current = current + count_x;
    }
}

float get_brightness(int pixel, int channels) {
    unsigned char r, g, b, a;
    get_rgba(pixel, &r, &g, &b, &a);
    const float inv_255 = 1.f/255.f;
    float brightness = 1.f-(r*inv_255 + g*inv_255 + b*inv_255)/clamp(channels, 1, 3) * (a*inv_255);
    return brightness;
}

// sample_size == 1
static char get_char(image* img) {
    float brightness = get_brightness(get_pixel_advance(img), img->channels);
    int ascii_count = sizeof(ascii_by_brightness)/sizeof(ascii_by_brightness[0]) - 1;
	return ascii_by_brightness[(int)(brightness*(ascii_count-1))];
}

// sample_size > 1
static char get_char_w(image* img, int** buf, int sample_size) {
    int x, y;
    get_current_pixel_xy(img, &x, &y);
    int dist_to_right = img->width - (x+sample_size);
    int dist_to_bottom = img->height - (y+sample_size);
    int count_x = sample_size - min(dist_to_right, 0);
    int count_y = sample_size - min(dist_to_bottom, 0);

    read_pixels2d(img, buf, count_x, count_y);

    float brightness = 0;

    for(int j=0;j<sample_size;j++) {
        for(int i=0; i<sample_size; i++) {
            brightness += get_brightness(buf[j][i], img->channels);
        }
    }

    brightness /= sample_size*sample_size;
    int ascii_count = sizeof(ascii_by_brightness)/sizeof(ascii_by_brightness[0]) - 1;
    return ascii_by_brightness[(int)(brightness*(ascii_count-1))];
}


static void write_raster_to_file(image* img, FILE* file, int sample_size) {
    char* line_buf;
	if(sample_size == 1) { 
        line_buf = malloc((img->width + 1) * sizeof(char));
        line_buf[img->width] = '\0';
        for(int j=0;j<img->height;j+=1) {
            for(int i=0;i<img->width;i+=1) {
                line_buf[i] = get_char(img);
            }
            fprintf(file, "%s\n", line_buf);
        }
	}else {
        int** buf = malloc(sizeof(int*) * sample_size); //buf that will collect sample_size*sample_size pixel data
        for(int i=0;i<sample_size;i++) {
            buf[i] = malloc(sizeof(int) * sample_size);
        }
        int x_len = (img->width-1)/sample_size + 1;
        int y_len = (img->height-1)/sample_size + 1;
        int line_buf_len = x_len;
        line_buf = malloc((line_buf_len + 1) * sizeof(char));
        line_buf[line_buf_len] = '\0';
        for(int j=0;j<y_len;j+=1) {
            for(int i=0;i<x_len;i+=1) {
                line_buf[i] = get_char_w(img, buf, sample_size);
            }
            fprintf(file, "%s\n", line_buf);
        }
        for(int i=0;i<sample_size;i++) {
            free(buf[i]);
        }
        free(buf);
	}
    free(line_buf);
    
}

int raster_to_ascii(char* image_name, char* file_out_name, int sample_size) {
    assert(sample_size >= 1, "Sample size can't be lower than 1");
    int width, height, channels;
    unsigned char *stb_img = stbi_load(image_name, &width, &height, &channels, 0);
    if (stb_img == NULL) {
        printf("Failed to load image\n");
        return -1;
    }

    assert(channels <= 4, "Cant convert image with more than 4 channels");

    if(strcmp(file_out_name, "") == 0 || strcmp(file_out_name, image_name) == 0) {
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

    write_raster_to_file(&img, file_out, sample_size);

    fclose(file_out);
    stbi_image_free(stb_img);
    return 0;
}