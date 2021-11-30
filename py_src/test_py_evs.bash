#!/bin/bash

python3 test_write_event.py \
    | python3 async_py_simul_three.py \
    | python3 test_read_event.py
