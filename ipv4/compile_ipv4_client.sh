rawnetcc ./ipv4_client ipv4_client.c ipv4.c ipv4_config.c ipv4_route_table.c ../arp/arp.c ../eth/eth.c
rawnetcc ./ipv4_server ipv4_server.c ipv4.c ipv4_config.c ipv4_route_table.c ../arp/arp.c ../eth/eth.c
chmod +x ./ipv4_client
chmod +x ./ipv4_server