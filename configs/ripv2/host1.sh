#!/bin/bash
# Configuración para H1
ip addr flush dev eth1
ip addr add 10.0.1.10/24 dev eth1
ip link set eth1 up

# Ruta hacia la red 10.0.0.0/8 a través de R1
ip route add 10.0.0.0/8 via 10.0.1.1 dev eth1
# O simplemente el default gateway:
# ip route add default via 10.0.1.1