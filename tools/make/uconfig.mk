uconfig:
	@echo -e "\033[1;33m[*]\033[0m Building uconfig"
	@cd tools/uconfig && cargo build

menuconfig: uconfig 
	@echo -e "\033[1;33m[*]\033[0m Running uconfig (debug)"
	@./tools/uconfig/target/debug/uconfig

include/config.h:
	@echo -e "\033[1;91m[*]\033[0m no configuration!"
	@echo -e "\033[1;91m[*]\033[0m please generate the kernel configuration with \`make menuconfig\` or \`make defconfig\`!"
	@exit 1
