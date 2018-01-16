#!/bin/bash

microhard=enp0s20f0u5u3

socat UDP4-RECV:5555 UDP4-DATAGRAM:224.255.0.1:6666,ip-add-membership=224.255.0.1:$microhard
