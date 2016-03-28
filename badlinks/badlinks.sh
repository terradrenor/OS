#!/bin/bash

directory=$1

if ! [ -d $directory ];
then
    echo "Error: the specified directory doesn't exist."
    exit 1
fi

find -L $directory -mtime +7 -type l;
