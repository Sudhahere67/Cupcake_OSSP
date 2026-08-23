# Skill Exercise 1 – Process Abstraction and System Calls

## Objective

To understand Linux process abstraction, process creation, the exec() family,
parent-child relationships, process trees, and system call tracing.

## Programs

1. fork_demo.c – Demonstrates fork() and parent-child processes.
2. exec_demo.c – Demonstrates execution of another program using exec().
3. process_tree.c – Creates child processes and displays their relationships.

## Compilation

make

## Execution

./fork_demo
./exec_demo
./process_tree

## System Call Tracing

strace ./fork_demo

## Tools Used

Linux Terminal
GCC
Git
Make
strace

## Outcome

The exercises demonstrate how Linux creates processes, executes programs,
manages parent-child relationships, and provides services through system calls.