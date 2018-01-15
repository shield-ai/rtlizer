#!/bin/bash

skywalker=192.168.0.4
port=5555
fake_data=false

if [ "$fake_data" = true ]; then
        while true; do
                shuf -i 1-100 -n 1 | socat - UDP4-SENDTO:$skywalker:$port
        done
else
        /home/debian/rtlizer/server/rtlizer_server
fi
