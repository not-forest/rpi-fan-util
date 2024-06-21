#!/bin/bash

# Small compilation script for user space utility.

# This is a user-space program, so only compiler would differ.
#COMPILER=gcc
COMPILER=aarch64-linux-gnu-gcc

$COMPILER -g3 -O3 -Wall src/main.c -o rpi_fan_util
