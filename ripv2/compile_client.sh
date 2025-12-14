#!/bin/bash

rawnetcc /tmp/ripv2_client ripv2_client.c ripv2_route_table.c ripv2.c \
    ../udp/udp.c \
    ../ipv4/ipv4.c ../ipv4/ipv4_config.c ../ipv4/ipv4_route_table.c \
    ../arp/arp.c \
    ../eth/eth.c \
    ../utils/rng.c

rawnetcc /tmp/ripv2_server ripv2_server.c ripv2_route_table.c ripv2.c \
    ../udp/udp.c \
    ../ipv4/ipv4.c ../ipv4/ipv4_config.c ../ipv4/ipv4_route_table.c \
    ../arp/arp.c \
    ../eth/eth.c \
    ../utils/rng.c