#include <types.h> 
#include <drivers/vga.h>
#include <drivers/screen.h>

#define UTOPIA_VERSION_MAJOR "1"
#define UTOPIA_VERSION_MINOR "0"
#define UTOPIA_VERSION_PATCH "0"
#define UTOPIA_VERSION \
    "Utopia beta@" \
    UTOPIA_VERSION_MAJOR "." UTOPIA_VERSION_MINOR "." UTOPIA_VERSION_PATCH

void kmain() {
    vga_clearscreen();
    printk("Core", "%s", UTOPIA_VERSION);
    printk("Core", "Remember to install Linux.");
}
