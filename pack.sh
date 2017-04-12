#!/bin/sh
mkdir target
mkdir target/pa1
rm target/pa1/*
cp src/main.c headers/*.h headers/*/*.h target/pa1
cd target
tar -cvzf pa1.tar.gz pa1
echo "all:
\tclang -std=c99 -Wall -Werror -pedantic *.c" > pa1/Makefile
