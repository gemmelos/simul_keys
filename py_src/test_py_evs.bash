#!/bin/bash

python3 /app/write_event.py \
    | python3 /app/py_simul_three.py \
    | python3 /app/print_event.py
