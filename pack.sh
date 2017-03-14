#!/bin/sh
mkdir target
mkdir target/pa1
rm target/pa1/*
cp src/*.c headers/*.h headers/*/*.h target/pa1
tar -cvzf target/pa1.tar.gz target/pa1
#clang -std=c99 -Wall -Werror -pedantic main.c
