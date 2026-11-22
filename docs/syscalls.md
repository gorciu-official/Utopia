# Utopia's native syscall ABI documentation

Utopia's syscall stack is meant to be portable, allowing for easy introduction of
new syscall ABIs into the kernel. Along from the Linux syscall ABI compatibility 
layer, Utopia implements its own set of syscalls, which are described in this 
document.

## Introduction

Utopia's native syscall ABI is architecture-independent, meaning there are the same 
syscall numbers, argument ordering and structure layouts on every architecture. There 
is a max number of registers that can hold an syscall argument, currently set to 6. 
The common syscall handler translates the architecture-dependent registers into 
`arg<1-6>`, which are then passed to the procedure handling the given syscall number.

## Register ABI

For the reasoning behind choosing those registers, you may want to read the
[old document describing this ABI](./outdated/syscalls.md).

### x86_64

**Syscall number and return value** in `rax`.
**Arguments** in `rdx`, `rdi`, `rsi`, `r10`, `r9`, `r8`.

### RISC-V 64

**Syscall number and return value** in `a0`.
**Arguments** in `a1`, `a2`, `a3`, `a4`, `a5`, `a7`.

## Syscalls

### Filesystem

The reserved block for this category of syscalls is `0`-`999`.

- 0: reserved for `open`

- 1: reserved for `close`

- 2: reserved for `position` (moves the position in an open file or gets it)

- 3: reserved for `readfile`

- 4: reserved for `readdir`

- 5: `write` - writes to the FD `arg1` the buffer `arg2` of size `arg3`

### A reserved block yay

For historical reasons, the `1000`-`1999` is reserved. It is most likely a new 
syscall category will occupy this block once it will be added.

### Process management

The reserved block for this category of syscalls is `2000`-`2999`.

- 2000: `exit` - exits the current process with the exit code `arg1`.

- 2001: `spawn` - spawns a process with an executable from path `arg1` using `arg2` syscall 
  convention (where `0` means default and `1` means Linux). `arg3` is a pointer to Unix-like 
  NUL-terminated `argv`; `arg4` is `envp`.

- 2002: `become` - replaces the current process with an executable from path `arg1` using 
  `arg2` syscall convention. `arg3` and `arg4` mean the same as in the previous syscall.

## Error handling 

Syscalls return a negative value in the architecture-specific `return value` 
register if something wents wrong. 

Specifically, one of these values:
- `-1` (`ERR_NOT_IMPLEMENTED`): the syscall does not exist or a specific argument-defined 
  function passes validation but is unimplemented

- `-2` (`ERR_INVALID_ARGUMENT`): an invalid argument to a syscall 

- `-3` (`ERR_NO_FILE`): either no such file descriptor or no such file/directory was found

- `-4` (`ERR_BAD_FS_NODE_TYPE`): expected directory, but given a file and similar things

- `-5` (`ERR_PERMISSION_DENIED`): no capability to perform operation X

- `-6` (`ERR_BUFFER_TOO_SMALL`): rerun the syscall with a bigger buffer

- `-7` (`ERR_MESSAGE_TOO_LARGE`): the size given is too large

- `-8` (`ERR_BAD_EXEC`): could not load executable

- `-9` (`ERR_NO_MEMORY`): the kernel ran out of memory while handling the syscall

Some of those shall be renamed, as it is unclear what they do / they get used
more often than in a specified situation - e.g. `ERR_MESSAGE_TOO_LARGE` is being
used even when the size does not refer to a message.
