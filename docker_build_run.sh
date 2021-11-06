#!/bin/bash

docker build -t test_simul . \
    && docker run -it test_simul