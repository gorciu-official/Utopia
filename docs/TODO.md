# ToDO list

A quite detailed ToDO list that I'm focusing on completing recently. I've decided to start writing it down though.

You may find some other tasks that can be done with the `git grep -ir "TODO"` command.

## Software porting

- [X] `nsh` ([this thingy](https://github.com/na-razie-bez-nicku/nsh/tree/main))
- [X] `bash`
- [ ] telnet client from `inetutils`

## Internal tasks 

- [X] Finally fix GRUB's version issues
- [ ] Add pseudo-randomness source for `getrandom`
- [ ] Expand support for Utopia syscall ABI
    - [ ] Add capabilities
- [ ] Build a common device tree abstraction for device discovery
    - [ ] x86_64's PCI
    - [ ] RISC-V 64's device tree
- [ ] TCP/IP
- [ ] Improve clock stack
    - [ ] Replace legacy `PIT`
    - [ ] Find a better clock source for calibrating LAPIC
- [X] Improve keyboard stack
