# TODO: Cambiar localizacion del binario
rawnetcc ./ripv2_client ripv2_client.c \
    ../udp/udp.c \
    ../ipv4/ipv4.c ../ipv4/ipv4_config.c ../ipv4/ipv4_route_table.c \
    ../arp/arp.c \
    ../eth/eth.c \
    ../utils/rng.c