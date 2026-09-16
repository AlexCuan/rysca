#ifndef _ETH_H
#define _ETH_H

#include <stdint.h>

/* Size in bytes of a MAC address (48 bits == 6 bytes) */
#define MAC_ADDR_SIZE 6

/* Type used to store MAC addresses */
typedef unsigned char mac_addr_t[MAC_ADDR_SIZE];

#define ETH_TYPE_IPV4 0x0800
#define ETH_MIN_PAYLOAD 46

/* Broadcast MAC address: "FF:FF:FF:FF:FF:FF" */
extern mac_addr_t MAC_BCAST_ADDR;

/* Length in bytes of a string representing a MAC address */
#define MAC_STR_LENGTH 18

/* Maximum Transmission Unit (MTU) of Ethernet frames. */
#define ETH_MTU 1500

/* Handle for an Ethernet interface. This is an opaque structure that must not
   be accessed directly, but through the functions of this library. */
typedef struct eth_iface eth_iface_t;


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
eth_iface_t* eth_open(char* ifname);


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
char* eth_getname(eth_iface_t* iface);


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
void eth_getaddr(eth_iface_t* iface, mac_addr_t addr);


/* int eth_send
 * ( eth_iface_t * iface,
 *   mac_addr_t dst, uint16_t type, unsigned char * data, int data_len );
 *
 * DESCRIPTION:
 *   This function sends an Ethernet frame through the given interface.
 *
 * PARAMETERS:
 *       'iface': Handle of the Ethernet interface the packet is to be sent
 *                through.
 *                The interface must have been initialised with 'eth_open()'
 *                beforehand.
 *         'dst': MAC address of the destination host.
 *        'type': Value of the 'Type' field of the Ethernet frame to send.
 *     'payload': Pointer to the data to send in the Ethernet frame.
 * 'payload_len': Length in bytes of the data to send.
 *
 * RETURN VALUE:
 *   The number of data bytes that could be sent.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred.
 */
int eth_send
(eth_iface_t* iface,
 mac_addr_t dst, uint16_t type, unsigned char* payload, int payload_len);


/* int eth_recv
 * ( eth_iface_t * iface,
 *   mac_addr_t src, uint16_t type, unsigned char buffer[], long int timeout );
 *
 * DESCRIPTION:
 *   This function obtains the next packet received by the given Ethernet
 *   interface. The operation may wait indefinitely or for a limited time
 *   depending on the 'timeout' parameter.
 *
 *   This function can only receive packets from a single interface. To listen
 *   on several Ethernet interfaces simultaneously, use the 'eth_poll()'
 *   function.
 *
 * PARAMETERS:
 *    'iface': Handle of the Ethernet interface a packet is to be received
 *             from.
 *             The interface must have been initialised with 'eth_open()'
 *             beforehand.
 *      'src': MAC address of the host that sent the received Ethernet frame.
 *             This is an output parameter. The address is copied into the
 *             given memory, which must have been reserved beforehand.
 *     'type': Value of the 'Type' field of the Ethernet frame to receive.
 *             Frames with a different 'type' value are discarded.
 *   'buffer': Array where the data of the received frame will be stored.
 *  'buf_len': Length of the 'buffer' where the data of the received frame will
 *             be stored. If more data is received than fits in 'buffer', only
 *             the first 'buf_len' bytes are copied.
 *  'timeout': Time in milliseconds to wait for a frame before returning. A
 *             negative number means waiting indefinitely, while with a '0' the
 *             function returns immediately, whether a frame was received or
 *             not.
 *
 * RETURN VALUE:
 *   The length in bytes of the data of the received frame (which may be
 *   greater than 'buf_len'), or '0' if no frame was received because the timer
 *   expired.
 *
 * ERRORS:
 *   The function returns '-1' if an error occurred.
 */
int eth_recv
(eth_iface_t* iface, mac_addr_t src, uint16_t type, unsigned char buffer[],
 int buf_len, long int timeout);


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
(eth_iface_t* ifaces[], int ifnum, long int timeout);


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
int eth_close(eth_iface_t* iface);


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
void mac_addr_str(mac_addr_t addr, char str[]);

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
int mac_str_addr(char* str, mac_addr_t addr);


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
void print_pkt(unsigned char* packet, int pkt_len, int hdr_len);

#endif /* _ETH_H */
