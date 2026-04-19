#include <types.h> 
#include <drivers/vga.h>
#include <drivers/screen.h>

void kmain() {
    vga_clearscreen();
    printf("\x1b[32msiema mordo uzytkowniku numer %d\n\x1b[0m%s: %d", 42, "mordo zapamietaj ten numer", 32); 
}
