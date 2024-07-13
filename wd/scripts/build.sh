#!/bin/bash

${CROSS_COMPILE}gcc bbEnv.c -I ../../arch/arm/include/string.h -o bbEnv -static
