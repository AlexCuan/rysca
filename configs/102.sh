#!/bin/bash

# --- Network Configuration Script ---
# This script configures the network interface 'eth1'.
# It will:
# 1. Remove all existing IP addresses from eth1.
# 2. Assign the static IP address 192.100.100.101 with a /24 subnet mask.
# 3. Set the default gateway to 192.100.100.102.
#
# IMPORTANT: This script must be run with root privileges (e.g., using sudo).

# Define variables for easier modification
INTERFACE="eth1"
IP_ADDRESS="192.100.100.101"
SUBNET_MASK="24" # /24 is equivalent to 255.255.255.0
GATEWAY="192.100.100.102"

echo "--- Starting network configuration for $INTERFACE ---"

# Check if the script is run as root
if [ "$(id -u)" -ne 0 ]; then
  echo "This script must be run as root. Please use sudo." >&2
  exit 1
fi

# 1. Bring the interface down to safely change its configuration
echo "Bringing interface $INTERFACE down..."
ip link set dev $INTERFACE down

# 2. Flush (wipe) the old IP address configuration
echo "Wiping existing IP configuration from $INTERFACE..."
ip addr flush dev $INTERFACE

# 3. Assign the new static IP address and subnet mask
echo "Assigning IP $IP_ADDRESS/$SUBNET_MASK to $INTERFACE..."
ip addr add $IP_ADDRESS/$SUBNET_MASK dev $INTERFACE

# 4. Bring the interface back up
echo "Bringing interface $INTERFACE up..."
ip link set dev $INTERFACE up

# 5. Remove any existing default routes (optional, but good practice)
echo "Removing existing default routes..."
ip route del default > /dev/null 2>&1 || true # Ignore errors if no default route exists

# 6. Add the new default route (gateway)
echo "Setting default gateway to $GATEWAY..."
ip route add default via $GATEWAY dev $INTERFACE

echo "--- Configuration complete! ---"
echo "Interface $INTERFACE is now configured with IP $IP_ADDRESS and gateway $GATEWAY."
echo "You can verify the changes with 'ip addr show $INTERFACE' and 'ip route show'."

exit 0

