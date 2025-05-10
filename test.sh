#!/bin/bash
pkill -f "complete_system\|test_tcp" || true
sleep 1
./run_system.sh > system_debug_$(date +%Y%m%d_%H%M%S).log 2>&1 &
sleep 5
cd build/bin && ./log_generator_example --rate 2 --duration 5 --network --target 127.0.0.1:8001 --console
