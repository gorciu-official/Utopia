# Syscall convention

## Registers & argument passing convention

### x86_64

**Syscall number and return value** in `rax`.
**Arguments** in `rdx`, `rdi`, `rsi`, `r10`, `r9`, `r8`.

**Reasoning**: `rax` is the first register we can use, so it goes towards storing syscall number and return value. Using `rbx` is a bad idea, so the next register available to use would be `rcx`. This one is overwritten by the `syscall`/`sysret` so we can't use it. Next one is `rdi`, which we can use. Also, `rdi` has a `d` in it, so it has a connection or smth, that's why we'll use this one now. Then every register from `r10` to `r8`. Yes. I'm a perfectionist.
 
### RISC-V 64

**Syscall number and return value** in `a0`.
**Arguments** in `a1`, `a2`, `a3`, `a4`, `a5`, `a7`

**Reasoning**: it's linear. kind of.

## Syscalls 

For simplicity we will refer to architecture-specific registers as `arg<1-6>`. It's also how it's handled internally.

This will be a stable interface once merged to `main`, but now shouldn't be treated as one.

### Filesystem

Reserved block: `0`-`999`.

- 0: `open` - opens a file. `arg1` points to a parent directory, can be `NULL` if relative to the current directory. `arg2` is the pointer to a NUL-terminated name of file to open. `arg3` holds a flags register (currently reserved, set to 0). returns a file descriptor.
- 1: `close` - closes the file. `arg1` is the file descriptor.
- 2: `position` - returns the current/changed position in the file. if `arg2` == `1`, changes the position in the file to `arg1`.
- 3: `readfile` - reads the file with a file descriptor of `arg1` with the limit of `arg2` bytes. increases position in the file by the number of read bytes.
- 4: `readdir` - writes an array of directory entries of a directory described by `arg3` with the maximum array length in bytes of `arg2` to `arg1`. returns the size of the array. increments position by number of read nodes.
- 5: `write` - writes `arg2` amount of bytes to the file described by `arg1`
- 6: `nodeinfo` - reads the current `file_info_t` to a pointer in `arg1` 

Even though filesystem syscalls could be implemented through `IPC` I've decided to dedicate a section for them, mostly because the microkernel provides a few essential filesystems needed during boot. And they are one of the most common operations.

### IPC 

Reserved block: `1000`-`1999`.

- 1000: `send` - sends a message. `arg2` is a `void*` pointer to a message, `arg3` is its size and `arg1` tells which process to send the message to.
- 1001: `receive` - receives messages to `arg1` which is a pointer to an array of `ipc_message_t`.

**Considering**:
- `call` - a syscall that would be used to send a message and wait for a reply back. worried about deadlocking though. there is probably no way the microkernel could enforce that the called process would respond. a quite good solution would be to add a timeout argument ???

## Error handling 

Syscalls return a negative value in the architecture-specific `return value` register if something wents wrong. 

Specifically, one of these values:
- `-1` (`ERR_NOT_IMPLEMENTED`): the syscall does not exist or a specific argument-defined function passes validation but is unimplemented
- `-2` (`ERR_INVALID_ARGUMENT`): an invalid argument to a syscall 
- `-3` (`ERR_NO_FILE`): either no such file descriptor or no such file/directory was found
- `-4` (`ERR_BAD_FS_NODE_TYPE`): expected directory, but given a file and similar things
- `-5` (`ERR_PERMISSION_DENIED`): no capability to perform operation X
- `-6` (`ERR_BUFFER_TOO_SMALL`): rerun the syscall with a bigger buffer
- `-7` (`ERR_MESSAGE_TOO_LARGE`): the size given is too large (used mostly in IPC)

## Structures

```c
typedef enum {
    FILE_FLAG_DIRECTORY = (1 << 1),
    FILE_FLAG_FILE = (1 << 0)
} file_flags_t;

typedef struct {
    uint64_t struct_size;
    file_flags_t flags;
    char name[];
} file_info_t;

typedef struct {
    uint64_t pid;
    uint8_t msg[4096];
} ipc_message_t;
```
