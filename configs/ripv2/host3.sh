#!/bin/bash
# Configuración para H3
ip addr flush dev eth1
ip addr add 10.0.3.10/24 dev eth1
ip link set eth1 up

# Ruta hacia la red 10.0.0.0/8 a través de R1 (o podrías usar 10.0.3.2 para R2)
ip route add 10.0.0.0/8 via 10.0.3.1 dev eth1