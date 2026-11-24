# Instructions for agents working in this repository

I decided to write this file as using agents to work on certain parts of the project has become 
more and more useful. Unfortuneally agents often do not seem to understand how to work with
certain parts of OSDev projects, making them pretty annoying without an `AGENTS.md`. This file 
gives instructions for clankers that should *hopefully* make them better at working on boilerplate
inside this project.

## Project setup

1. All agents should read the `docs/contributing.md` and `docs/building.md` files and follow
   all instructions written there.

2. Before working on the repository, agents are required to set up Rust and C development 
   enviorments inside sandboxes they are working within. They should use a package manager
   provided by their distribution (usually `apt`) to install packages; alternatively use 
   official bash scripts if no package is available.

3. Apart from installing compilers, agents are advised to install QEMU and test their changes
   by pairing QEMU in headless mode (`-display none`) with the `timeout` command.

4. If a development enviorment set up fails, inform the user and ask whether they want to 
   continue agents working on the repository without these tools.

## Writing code

1. Avoid generic AI slop comments as written in `docs/contributing.md`.

2. Avoid oversimplifying the solution; if something requires additional steps and it can be 
   done quickly, do them, otherwise ask the user.

3. Keep the code simple and readable, which will improve maintainability over time. This 
   includes splitting the code into multiple files, keeping functions tiny and other 
   things like this. The need for comments explaining the code often means bad code, so 
   please avoid them and instead restructure the code to be more readable.

## Debugging instructions

1. You have plenty of tools to use to debug the behaviour of the system, like debug mode 
   a.k.a. `make run-dbg SMP_CORES=1` that launches GDB alongside QEMU or the 
   `Print messages in extra debugging level` option in `Uconfig`. Utilize these tools if 
   you are unable to figure out the error on your own.

2. If an user pastes a log after you have generated a patch, it will most certainly mean
   that something broke **after your patch**. Focus on improving your changes, not on 
   finding unrelated bugs in unrelated part of the repository (unless you are certain
   your changes uncovered a bug somewhere else).

## Finishing up

1. Depending on where you are working, you should present a modified copy of the repository
   in one of two ways:
   - **working on a developer's computer**: you just edit the files like you normally would
     and you do not need to do anything else. **Do not commit**.
   - **working inside a remote sandbox**: use `git format-patch` to make patchfiles, then 
     present them to an user
