# Cross Compiler and On-Chip Debugging Demo

This project demonstrates a simple cross-compilation and remote debugging workflow for an AArch64 Linux target, such as Raspberry Pi 4.

The demo application is a POSIX thread based real-time scheduler simulator. It creates periodic tasks, measures jitter and execution time, detects deadline misses, and exposes debug breakpoints for remote inspection with GDB or GDBserver.

## Contents

- `CrossCompiler_OCD.pdf` - presentation about cross compilers and on-chip debugging concepts.
- `Demo/main.c` - C source code for the scheduler demo.
- `Demo/komutlar.txt` - build, transfer, and debug commands.
- `Demo/scheduler_rpi4` - compiled target binary.

## Requirements

Host machine:

- AArch64 cross compiler: `aarch64-linux-gnu-gcc`
- AArch64 GDB: `aarch64-unknown-linux-gnu-gdb`
- SSH/SCP access to the target device

Target device:

- AArch64 Linux system
- `gdbserver`

## Build

Run this command on the host machine inside the `Demo` directory:

```bash
aarch64-linux-gnu-gcc -g3 -Og -fno-omit-frame-pointer -Wall -Wextra -pthread main.c -o scheduler_rpi4
```

## Transfer to Target

Replace the username and IP address with your target device information:

```bash
scp ./scheduler_rpi4 user@target-ip:~/
```

Example:

```bash
scp ./scheduler_rpi4 b23i@10.30.90.140:~/
```

## Start GDB Server on Target

Run this on the target device:

```bash
gdbserver :3333 ./scheduler_rpi4
```

## Connect from Host

Run this on the host machine:

```bash
aarch64-unknown-linux-gnu-gdb scheduler_rpi4 -ex "target remote target-ip:3333"
```

Example:

```bash
aarch64-unknown-linux-gnu-gdb scheduler_rpi4 -ex "target remote 10.30.90.140:3333"
```

## Program Behavior

The application starts three periodic tasks:

- `T1_10ms`
- `T2_50ms`
- `T3_100ms`

Each task records:

- average jitter
- maximum jitter
- average execution time
- maximum execution time
- deadline misses

When debug mode and breakpoints are enabled, the program triggers architecture-specific break instructions:

- `brk #0` for AArch64
- `bkpt #0` for ARM
- `__builtin_trap()` for other architectures

## Purpose

The goal of this project is to show how embedded software can be developed on a host computer, cross-compiled for a target device, transferred to that device, and debugged remotely using GDBserver.
