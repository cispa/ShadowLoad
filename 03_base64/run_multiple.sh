#!/usr/bin/env bash

set -e

make -B
rm -f results
for i in $(seq 1 "$1")
do
    taskset -c 0 ./sidechannel_base64 >> results
done
