#!/bin/bash

gcc -DTEST_MODE test.c ../pdp11.c ../pdp11_debug.c -o test
