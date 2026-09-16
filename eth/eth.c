#include "eth.h"
#include <rawnet.h>
#include <timerms.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

/* Broadcast MAC address: FF:FF:FF:FF:FF:FF */
mac_addr_t MAC_BCAST_ADDR = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Estructura del manejador del interfaz ethernet */
struct eth_iface
{
    rawiface_t* raw_iface; /* Handle of the "raw" interface */
    mac_addr_t mac_address; /* MAC address of the interface. Stored here rather
                             than queried from the "raw" interface to avoid an
                             extra system call every time a frame is sent. */
};

/* Size of the Ethernet header (not including the FCS field) */
#define ETH_HEADER_SIZE 14
/* Maximum size of an Ethernet frame (not including the FCS field) */
#define ETH_FRAME_MAX_LENGTH (ETH_HEADER_SIZE + ETH_MTU)

/* Header of an Ethernet frame */
struct eth_frame
{
    mac_addr_t dest_addr; /* Destination MAC address */
    mac_addr_t src_addr; /* Source MAC address */
    uint16_t type; /* 'Type' field.
                           Identifier of the upper network layer */
    unsigned char payload[ETH_MTU]; /* 'payload' field.
                                          Data of the upper layer */

    /* NOTE: the "Frame Checksum" (FCS) field is not included in the structure
       because the network card adds it automatically. */
};


/* eth_iface_t * eth_open ( char* ifname );
 *
 * DESCRIPTION:
 *   This function initialises the specified Ethernet interface so that it can
 *   be used by the remaining functions of the library.
 *
 *   The memory of the returned interface handle must be released with the
 *   'eth_close()' function.
 *
 * PARAMETERS:
 *   'ifname': String with the name of the Ethernet interface to initialise.
 *
 * RETURN VALUE:
 *   Handle of the initialised Ethernet interface.
 *
 *   That handle is a pointer to an opaque structure that must not be accessed
 *   directly; use the functions of the library instead.
 *
 * ERRORS:
 *   The function returns 'NULL' if an error occurred.
 */
eth_iface_t* eth_open(char* ifname)
{
    struct eth_iface* eth_iface;

    /* Reserve memory for the Ethernet interface handle */
    eth_iface = malloc(sizeof(struct eth_iface));
    if (eth_iface == NULL)
    {
        fprintf(stderr, "eth_open(): ERROR en malloc()\n");
        return NULL;
    }

    /* Open the underlying "raw" interface */
    rawiface_t* raw_iface = rawiface_open(ifname);
    if (raw_iface == NULL)
    {
        fprintf(stderr, "eth_open(): ERROR en rawiface_open(): %s\n",
                rawnet_strerror());
        free(eth_iface);
        return NULL;
    }
    eth_iface->raw_iface = raw_iface;

    /* Copy the MAC address into the handle */
    rawiface_getaddr(raw_iface, eth_iface->mac_address);

    return eth_iface;
}


/* char * eth_getname ( eth_iface_t * iface );
 *
 * DESCRIPTION:
 *   This function returns the name of the specified Ethernet interface.
 *
 * PARAMETERS:
 *   'iface': Handle of the Ethernet interface whose name is requested.
 *            The interface must have been initialised with 'eth_open()'
 *            beforehand.
 *
 * RETURN VALUE:
 *   String with the name of the interface.
 *
 * ERRORS:
 *   The function returns 'NULL' if the interface was not initialised
 *   correctly.
 */
char* eth_getname(eth_iface_t* iface)
{
    char* iface_name = NULL;

    if (iface != NULL)
    {
        iface_name = rawiface_getname(iface->raw_iface);
    }

    return iface_name;
}

/* void eth_getaddr ( eth_iface_t * iface, mac_addr_t addr );
 *
 * DESCRIPTION:
 *   This function obtains the MAC address of the specified Ethernet
 *   interface.
 *
 * PARAMETERS:
 *   'iface': Handle of the Ethernet interface whose address is requested.
 *            The interface must have been initialised with 'eth_open()'
 *            beforehand.
 *    'addr': Array where the MAC address of the Ethernet interface will be
 *            copied. MAC addresses take up 'MAC_ADDR_SIZE' bytes.
 */
void eth_getaddr(eth_iface_t* iface, mac_addr_t addr)
{
    if (iface != NULL)
    {
        memcpy(addr, iface->mac_address, MAC_ADDR_SIZE);
    }
}

int eth_send
(eth_iface_t* iface,
 mac_addr_t dst, uint16_t type, unsigned char* payload, int payload_len)
{
    int bytes_sent;

    /* Check the parameters */
    if (iface == NULL)
    {
        fprintf(stderr, "eth_send(): ERROR: iface == NULL\n");
        return -1;
    }

    /* The payload is copied into an ETH_MTU byte buffer: without this check an
       upper layer that does not fragment overflows the stack. */
    if ((payload == NULL) || (payload_len < 0) || (payload_len > ETH_MTU))
    {
        fprintf(stderr, "eth_send(): ERROR: longitud de payload invalida (%d)\n",
                payload_len);
        return -1;
    }

    struct eth_frame eth_frame;
    memcpy(eth_frame.dest_addr, dst, MAC_ADDR_SIZE);
    memcpy(eth_frame.src_addr, iface->mac_address, MAC_ADDR_SIZE);
    eth_frame.type = htons(type);

    memcpy(eth_frame.payload, payload, payload_len);

    /* --- START OF PADDING LOGIC --- */
    int final_payload_len = payload_len;

    if (payload_len < ETH_MIN_PAYLOAD)
    {
        // Work out how many padding bytes are missing
        int padding_len = ETH_MIN_PAYLOAD - payload_len;

        // Fill the leftover part of the payload buffer with zeros
        memset(eth_frame.payload + payload_len, 0, padding_len);

        // Update the final length that will be sent
        final_payload_len = ETH_MIN_PAYLOAD;

#ifdef NET_DEBUG
        printf("[ETH] Padding applied: Payload %d -> %d bytes\n", payload_len, final_payload_len);
#endif
    }
    /* --- END OF PADDING LOGIC --- */

    int eth_frame_len = ETH_HEADER_SIZE + final_payload_len;

    /* Note: eth_frame_len now includes the padding if it was needed */
    bytes_sent = rawnet_send
        (iface->raw_iface, (unsigned char*)&eth_frame, eth_frame_len);

    if (bytes_sent == -1)
    {
        fprintf(stderr, "eth_send(): ERROR en rawnet_send(): %s\n",
                rawnet_strerror());
        return -1;
    }

    /* Return the number of useful data bytes sent (not counting the padding) */
    return payload_len;
}

int eth_recv
(eth_iface_t* iface, mac_addr_t src, uint16_t type, unsigned char buffer[],
 int buf_len, long int timeout)
{
    int payload_len;

    if (iface == NULL)
    {
        fprintf(stderr, "eth_recv(): ERROR: iface == NULL\n");
        return -1;
    }


    /* Initialise a timer so the timeout is honoured even if frames with the
       wrong type are received. */
    timerms_t timer;
    timerms_reset(&timer, timeout);

    int frame_len;
    int eth_buf_len = ETH_HEADER_SIZE + buf_len;
    unsigned char eth_buffer[eth_buf_len];
    struct eth_frame* eth_frame_ptr = NULL;
    int is_target_type;
    int is_my_mac;

    do
    {
        long int time_left = timerms_left(&timer);

        frame_len = rawnet_recv(iface->raw_iface, eth_buffer, eth_buf_len, time_left);

        if (frame_len < 0)
        {
            return -1;
        }
        else if (frame_len == 0)
        {
            return 0;
        }

        eth_frame_ptr = (struct eth_frame*)eth_buffer;


#ifdef NET_DEBUG
        if (ntohs(eth_frame_ptr->type) == 0x0800)
        {
            printf("[ETH DEBUG] Trama IP recibida. Dest MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   eth_frame_ptr->dest_addr[0], eth_frame_ptr->dest_addr[1],
                   eth_frame_ptr->dest_addr[2], eth_frame_ptr->dest_addr[3],
                   eth_frame_ptr->dest_addr[4], eth_frame_ptr->dest_addr[5]);
        }
#endif

        // Checks
        is_my_mac = (memcmp(eth_frame_ptr->dest_addr, iface->mac_address, MAC_ADDR_SIZE) == 0);
        is_target_type = (ntohs(eth_frame_ptr->type) == type);

        // MULTICAST FIX
        int is_multicast = (eth_frame_ptr->dest_addr[0] & 0x01);
        int is_broadcast = (memcmp(eth_frame_ptr->dest_addr, MAC_BCAST_ADDR, MAC_ADDR_SIZE) == 0);

        // Acceptance condition: for me, OR multicast, OR broadcast. And the type matches.
        if ((is_my_mac || is_multicast || is_broadcast) && is_target_type)
        {
            break; // ACCEPT THE PACKET
        }
    }
    while (1);

    /* Frame received with the given 'type'. Copy the data and source MAC. */
    memcpy(src, eth_frame_ptr->src_addr, MAC_ADDR_SIZE);
    payload_len = frame_len - ETH_HEADER_SIZE;
    if (buf_len > payload_len)
    {
        buf_len = payload_len;
    }
    memcpy(buffer, eth_frame_ptr->payload, buf_len);

    return payload_len;
}


/* int eth_poll
 * ( eth_iface_t * ifaces[], int ifnum, long int timeout );
 *
 * DESCRIPTION:
 *   This function waits for packets on multiple Ethernet interfaces
 *   simultaneously. When any of the given interfaces receives a frame, the
 *   function returns the first interface that has a frame ready to be received
 *   with the 'eth_recv()' function.
 *
 *   This operation may listen on the Ethernet interfaces indefinitely or for a
 *   limited time depending on the 'timeout' parameter.
 *
 * PARAMETERS:
 *  'ifaces': Array with the handles of the Ethernet interfaces to receive
 *            from.
 *            All the interfaces must have been initialised with 'eth_open()'
 *            beforehand.
 *   'ifnum': Number of interfaces present in the 'ifaces' array.
 * 'timeout': Time in milliseconds to wait for a frame before returning. A
 *            negative number means waiting indefinitely.
 *
 * RETURN VALUE:
 *   The index of the first interface [0, ifnum-1] that has a frame ready to be
 *   received, or '-2' if the timer expired.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred.
 */
int eth_poll
(eth_iface_t* ifaces[], int ifnum, long int timeout)
{
    int iface_index;

    /* Build the list of hardware interfaces */
    rawiface_t* raw_ifaces[ifnum];
    int i;
    for (i = 0; i < ifnum; i++)
    {
        raw_ifaces[i] = ifaces[i]->raw_iface;
    }

    /* Call rawnet_poll() and handle errors */
    iface_index = rawnet_poll(raw_ifaces, ifnum, timeout);
    if (iface_index == -1)
    {
        fprintf(stderr, "eth_poll(): ERROR en rawnet_poll(): %s\n",
                rawnet_strerror());
        return -1;
    }
    else if (iface_index == -2)
    {
        /* Timeout! */
        return -2;
    }

    return iface_index;
}


/* int eth_close ( eth_iface_t * iface );
 *
 * DESCRIPTION:
 *   This function closes the specified Ethernet interface and releases the
 *   memory of its handle.
 *
 * PARAMETERS:
 *   'iface': Handle of the Ethernet interface to close.
 *
 * RETURN VALUE:
 *   Returns 0 if the Ethernet interface was closed correctly.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred.
 */
int eth_close(eth_iface_t* iface)
{
    int err = -1;

    if (iface != NULL)
    {
        err = rawiface_close(iface->raw_iface);
        free(iface);
    }

    return err;
}


/* void mac_addr_str ( mac_addr_t addr, char str[] );
 *
 * DESCRIPTION:
 *   This function generates a string representing the given MAC address.
 *
 * PARAMETERS:
 *   'addr': The MAC address to represent as text.
 *    'str': Memory where the generated string is to be stored.
 *           At least 'MAC_STR_LENGTH' bytes must be reserved.
 */
void mac_addr_str(mac_addr_t addr, char str[])
{
    if (str != NULL)
    {
        sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }
}

/* int mac_str_addr ( char* str, mac_addr_t addr );
 *
 * DESCRIPTION:
 *   This function scans a string looking for a MAC address.
 *
 * PARAMETERS:
 *    'str': The string to process.
 *   'addr': Memory where the MAC address found is stored.
 *
 * RETURN VALUE:
 *   Returns 0 if the string represented a MAC address.
 *
 * ERRORS:
 *   The function returns -1 if the string did not represent a MAC address.
 */
int mac_str_addr(char* str, mac_addr_t addr)
{
    int err = -1;

    if (str != NULL)
    {
        unsigned int addr_int[MAC_ADDR_SIZE];
        int len = sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
                         &addr_int[0], &addr_int[1], &addr_int[2],
                         &addr_int[3], &addr_int[4], &addr_int[5]);

        if (len == MAC_ADDR_SIZE)
        {
            int i;
            for (i = 0; i < MAC_ADDR_SIZE; i++)
            {
                addr[i] = (unsigned char)addr_int[i];
            }
            err = 0;
        }
    }

    return err;
}


/* void print_pkt ( unsigned char * packet, int pkt_len, int hdr_len );
 *
 * DESCRIPTION:
 *   This function prints the contents of the given packet to standard output.
 *   In addition, the first 'hdr_len' bytes of the message are highlighted in a
 *   different colour to make them easier to read.
 *
 * PARAMETERS:
 *     'packet': Pointer to the contents of the packet to print.
 *    'pkt_len': Total length in bytes of the packet to print.
 *    'hdr_len': Number of leading bytes to highlight by printing them in a
 *               different colour. Use any value less than or equal to zero to
 *               disable this feature.
 */
void print_pkt(unsigned char* packet, int pkt_len, int hdr_len)
{
    if ((packet == NULL) || (pkt_len <= 0))
    {
        return;
    }

    int i;
    for (i = 0; i < pkt_len; i++)
    {
        if ((i % 8) == 0)
        {
            /* End of a line has been reached */
            if (i > 0)
            {
                /* Insert a line break */
                printf("\n");

                /* Switch back to the normal colour for the index */
                if (i <= hdr_len)
                {
                    printf("\033[0m");
                }
            }

            /* Print a hexadecimal index byte at the start of each line */
            printf("  0x%04x:", i);

            if (i < hdr_len)
            {
                /* Print the first bytes of the header in a different colour */
                printf("\033[1;34m");
            }
        }
        else if ((i % 4) == 0)
        {
            /* Print a separator between each pair of 4 bytes */
            printf(" ");
        }

        /* Return to the normal colour once 'hdr_len' ends */
        if (i == hdr_len)
        {
            printf("\033[0m");
        }

        /* Print each byte of the packet in hexadecimal */
        printf(" %02x", packet[i]);
    }

    /* The whole packet was header, restore the normal colour */
    if (pkt_len <= hdr_len)
    {
        printf("\033[0m");
    }

    printf("\n");
}
