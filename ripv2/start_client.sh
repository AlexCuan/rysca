#!/bin/bash

echo "Enter the IP address of the router to query:"
read -r IP_ADDRESS

echo "Do you want the (f)ull routing table or a (p)artial one? (f/p)"
read -r CHOICE

if [[ "$CHOICE" == "p" ]]; then
  echo "Partial routing table display is not implemented."
elif [[ "$CHOCHOICE" == "f" ]]; then
  /tmp/ripv2_client ../configs/final/ipv4_config_client.txt ../configs/final/ipv4_route_table_client.txt "$IP_ADDRESS"
else
  echo "Invalid choice. Please enter 'f' for full or 'p' for partial."
fi
