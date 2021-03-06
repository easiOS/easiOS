#ifndef H_TEXT
#define H_TEXT

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

enum vga_color
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};

void tinit(size_t width, size_t height, uint16_t* addr);
bool tisinit();
void tputc(char a, uint8_t color);
void tcls();
void tswrite(const char* str);
void tputcat(char a, uint8_t color, size_t x, size_t y);
uint8_t tgetcolor();
void tsetcolor(uint8_t fg, uint8_t bg);
const uint16_t* tgetbuf();

#endif
