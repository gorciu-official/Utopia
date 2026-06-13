#include <types.h>

extern void vga_clearscreen();
extern void vga_printstr(const char *str, uint8_t color);
extern void vga_printchar(char c, uint8_t color);
extern void vga_printchar_nolock(char c, uint8_t color);

extern void vga_lock();
extern void vga_unlock();
