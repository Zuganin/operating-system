#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input_file> <output_file>"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"

echo "Создание FIFO..."
gcc -o create create_fifo.c
./create

echo "Запуск process2..."
./process2 &
proc2_pid=$!

echo "Запуск process1..."
./process1 "$INPUT_FILE" "$OUTPUT_FILE"

wait $proc2_pid

echo "Оба процесса завершены."
