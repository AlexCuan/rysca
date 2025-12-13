#!/bin/bash

echo "Enter the IP address of the router to query:"
read -r IP_ADDRESS

echo "Do you want the (f)ull routing table or a (p)artial one? (f/p)"
read -r CHOICE

if [[ "$CHOICE" == "p" ]]; then
  echo "Enter the subnet to query (e.g. 10.0.1.0):"
  read -r SUBNET
  echo "Enter the mask (e.g. 255.255.255.0):"
  read -r MASK

  # Llamada con argumentos extra
  /tmp/ripv2_client ../configs/final/ipv4_config_client.txt ../configs/final/ipv4_route_table_client.txt "$IP_ADDRESS" "$SUBNET" "$MASK"

elif [[ "$CHOICE" == "f" ]]; then
  # Llamada estándar
  /tmp/ripv2_client ../configs/final/ipv4_config_client.txt ../configs/final/ipv4_route_table_client.txt "$IP_ADDRESS"

else
  echo "Invalid choice. Please enter 'f' or 'p'."
fi


  /tmp/ripv2_client ../configs/final/ipv4_config_client.txt ../configs/final/ipv4_route_table_client.txt  10.0.2.1 10.0.1.0 255.255.255.0