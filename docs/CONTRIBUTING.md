# Contributing

Contains information about code style and other similar things. For building information see [docs/BUILDING.md](../BUILDING.md) file.

## Code style

**Naming convention:** Use `snake_case` or `kebab-case` for naming files; `snake_case` for naming variables, functions and constants; `UPPER_SNAKE_CASE` is for compile-time constants and macros (read: thigns from cpp - C preprocessor).

**Indentation:** Tabs are bad, 2-space indentation is bad, 8-space indentation is also bad. Please use 4-space indentation whereever possible. Do not double-indent `switch` statements. 

**Other notes:** Star belongs to the type (it's cleaner).

## Good practices

1. Do not include `arch/<whatever>/<whatever>.h` outside from architecture-specific drivers and `src/arch` (common arch files like `arch/common.h` are encouraged instead).
2. Test your changes before commiting.

That's it for now.
