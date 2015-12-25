#!/bin/bash

PROJ_ROOT=../
cd $PROJ_ROOT/build/example
echo "-------- start server --------"
./server &
sleep 1
echo -e "\n-------- start sync client --------"
./sync_client
sleep 1
echo -e "\n-------- start async client --------"
./async_client
sleep 1
echo -e "\n-------- start future client --------"
./future_client
kill -s INT `ps -elf | grep './server' | grep -v grep | awk '{print $4}'`

