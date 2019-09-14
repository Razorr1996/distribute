#!/bin/sh
USAGE="Usage:\tpack.sh DIGIT"
case "$1" in
    [1-5])
        dir=pa"$1"
        mkdir target
        mkdir target/$dir
        rm target/$dir/*
        cp src/"$1"lab.c src/functions.c target/$dir #source code
        if [ "$1" = "2" ] || [ "$1" = "3" ]; then
            cp src/bank_robbery.c target/$dir
        fi
        if [ "$1" = "4" ]; then
            cp src/queue.c target/$dir
        fi
        cp headers/*.h headers/*/*.h target/$dir #headers
        cp library/*.so target/$dir #libs
        cd target
        echo "all:\n\tclang -o ${1}lab -std=c99 -Wall -Werror -pedantic -L. -lruntime *.c" > $dir/Makefile
        tar -cvzf $dir.tar.gz $dir;;
    *) echo "$USAGE";;
esac
