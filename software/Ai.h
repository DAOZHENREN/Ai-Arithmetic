#ifndef __Ai__H
#define __Ai__H
#include "tinymaix.h"

// 定义画布的宽度和高度
#define CANVAS_WIDTH 80
#define CANVAS_HEIGHT 24
// 定义字符图像的大小
#define CHAR_IMG_SIZE 28

// 定义图像的宽度、高度和通道数
#define IMAGE_WIDTH (28)
#define IMAGE_HEIGHT (28)
#define IMAGE_CHANNEL (1)

// 定义分类的数量
#define CLASS_N (14) // 0-9, +, -, *, /

// 函数声明
void Ai_init();
uint8_t Ai_run();
uint8_t parse_output(tm_mat_t *outs);
void clean_input_image();

#endif