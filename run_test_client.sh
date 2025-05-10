#!/bin/bash
cd "$(dirname "$0")"/build/bin
./test_tcp_client "$@"
