uconfig:
	@echo -e "\033[1;33m[*]\033[0m Building uconfig"
	@cd tools/uconfig && cargo build

menuconfig: uconfig 
	@echo -e "\033[1;33m[*]\033[0m Running uconfig (debug)"
	@./tools/uconfig/target/debug/uconfig
