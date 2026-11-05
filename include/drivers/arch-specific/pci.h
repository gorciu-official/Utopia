#pragma once

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64

void pcie_log(); // Written in Rust btw, check /src/rust/drivers/pci.rs for implementation 

#else
#error This file should not be included outside x86_64 specific code
#endif