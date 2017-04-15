#!/bin/sh
USAGE="Usage:\tpack.sh DIGIT"
case "$1" in
    [1-5])
        PA=pa"$1"
        mkdir target
        mkdir target/$PA
        rm target/$PA/*
        cp src/"$1"lab.c src/functions.c headers/*.h headers/*/*.h target/$PA
        cd target
        tar -cvzf $PA.tar.gz $PA
        echo "all:\n\tclang -std=c99 -Wall -Werror -pedantic *.c" > $PA/Makefile;;
    *) echo "$USAGE";;
esac