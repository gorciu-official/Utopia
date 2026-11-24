# Contributing

General information for contributors. I used to keep building information here, but it got moved into 
[docs/building.md](building.md), so make sure to check that out too.

## AI usage

[Skip this section if you do not plan on using AI in your contribution](#code-style)

I am not against the usage of AI if you know what you are actually generating (e.g. no slop like "make
binutils work make no mistakes") and after your peak agent finished you review the code it made. I will
know for certain whenever someone submits AI slop submissions to this repository, so don't even try.
Remember that AIs can't take responsibility for whatever they are submitting, so the human that has
instructed the agent to make such a contribution is responsible for it.

**Please instruct your agents to read [docs/agents.md](agents.md).**

## Code style

- **Naming convention:** Use `snake_case` or `kebab-case` for naming files; `snake_case` for naming variables, 
  functions and constants; `UPPER_SNAKE_CASE` is for compile-time constants and macros (read: things from da C 
  preprocessor).

- **Indentation:** Tabs are bad, 2-space indentation is bad, 8-space indentation is also bad. Please use 
  4-space indentation whereever possible. Do not double-indent `switch` statements. 

- **Comments:** Avoid comments at all. The best code is a self-documenting code, the need for comments most likely
  means you wrote bad code. Some kinds of comments are acceptable of course (like grouping, joke comments or 
  TODOs), but please do not overuse them.

- **Other notes:**
  - It is advised to use left pointer alignment, although this rule is not strictly enforced

## Good practices

- Please do not use inline ASM or architecture-dependent functions outside of `src/arch` or arch-specific
  `src/drivers`. Usage of such things is acceptable in places like the scheduler, but it is still not a good 
  practice.

- If you need more than one driver for one category (e.g. multiple keyboard backends), please build a common
  abstraction on top of these drivers to avoid endless if-else preprocessor statements in every caller. 

- If a function makes sense to be possible to disable, please add it into `Uconfig`.
