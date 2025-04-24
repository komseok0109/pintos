# Pintos Operating System Project

This repository contains my implementation and enhancements of the Pintos educational operating system, completed as part of the **CSED312: Operating Systems** course at **POSTECH**.
Pintos is a simple instructional operating system framework for the x86 architecture, designed to help students understand and implement core OS concepts from the ground up.

## Project Overview

The Pintos project is divided into three major parts:

### Project 1: Threads
- Implemented priority scheduling
- Handled priority donation to address priority inversion
- Designed an advanced scheduler using a Multi-Level Feedback Queue (MLFQ)

### Project 2: User Programs
- Implemented user process creation and ELF binary loading
- Handled system calls and user/kernel boundary management
- Validated syscall arguments and managed file descriptors

### Project 3: Virtual Memory
- Implemented demand paging and page fault handling
- Supported anonymous and file-backed pages
- Maintained supplemental page tables
