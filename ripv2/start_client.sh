#!/bin/bash

echo "Enter the IP address of the router to query:"
read -r IP_ADDRESS

echo "Do you want the (f)ull routing table or a (p)artial one? (f/p)"
read -r CHOICE

# --- NUEVO: Preguntar por el checksum ---
echo "Disable UDP Checksum validation? (y/n)"
read -r DISABLE_CS

EXTRA_ARGS=""
if [[ "$DISABLE_CS" == "y" ]]; then
    EXTRA_ARGS="-d"
fi
# ----------------------------------------

if [[ "$CHOICE" == "p" ]]; then
  echo "Enter the subnet to query (e.g. 10.0.1.0):"
  read -r SUBNET
  echo "Enter the mask (e.g. 255.255.255.0):"
  read -r MASK

  # Llamada con argumentos extra + el flag opcional al final
  /tmp/ripv2_client ../configs/final/ipv4_config_client.txt ../configs/final/ipv4_route_table_client.txt "$IP_ADDRESS" "$SUBNET" "$MASK" $EXTRA_ARGS

elif [[ "$CHOICE" == "f" ]]; then
  # Llamada estándar + el flag opcional al final
  /tmp/ripv2_client ../configs/final/ipv4_config_client.txt ../configs/final/ipv4_route_table_client.txt "$IP_ADDRESS" $EXTRA_ARGS

else
  echo "Invalid choice. Please enter 'f' or 'p'."
fi