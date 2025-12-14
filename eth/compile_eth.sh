#!/bin/bash

# Compilar Cliente Ethernet
rawnetcc /tmp/eth_client eth_client.c eth.c

# Compilar Servidor Ethernet
rawnetcc /tmp/eth_server eth_server.c eth.c

echo "Compilación completada: /tmp/eth_client y /tmp/eth_server"