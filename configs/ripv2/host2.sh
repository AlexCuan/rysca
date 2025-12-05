#!/bin/bash
# Configuración para H2
ip addr flush dev eth1
ip addr add 10.0.2.10/24 dev eth1
ip link set eth1 up

# Ruta hacia la red 10.0.0.0/8 a través de R2
ip route add 10.0.0.0/8 via 10.0.2.1 dev eth1