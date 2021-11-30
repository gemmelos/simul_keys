#!/bin/bash
ps au | rg udevmon | head -n -1 | awk '{print $2}' | xargs sudo kill -9
