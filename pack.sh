#!/bin/sh
USAGE="Usage:\tpack.sh DIGIT"
case "$1" in
    [1-5])
        dir=pa"$1"
        mkdir target
        mkdir target/$dir
        rm target/$dir/*
        cp src/"$1"lab.c src/functions.c target/$dir #source code
        cp headers/*.h headers/*/*.h target/$dir #headers
        cp library/*.so target/$dir #libs
        cd target
        tar -cvzf $dir.tar.gz $dir
        echo "all:\n\tclang -o ${1}lab -std=c99 -Wall -Werror -pedantic -L. -lruntime *.c" > $dir/Makefile;;
    *) echo "$USAGE";;
esac