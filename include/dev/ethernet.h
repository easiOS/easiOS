#ifndef H_DEV_ETHERNET
#define H_DEV_ETHERNET

#include <stdint.h>
#include <stddef.h>

#define PROT_IPV4 0x0008
#define PROT_ARP  0x0608

#define MAX_ETH_N 1

#define htons(A) ((((uint16_t)(A) & 0xff00) >> 8) | \
(((uint16_t)(A) & 0x00ff) << 8))
#define htonl(A) ((((uint32_t)(A) & 0xff000000) >> 24) | \
(((uint32_t)(A) & 0x00ff0000) >> 8) | \
(((uint32_t)(A) & 0x0000ff00) << 8) | \
(((uint32_t)(A) & 0x000000ff) << 24))
#define ntohs htons
#define ntohl htohl

typedef struct ethernet_device eth_dev_t;

struct ethernet_device {
  uint32_t flags;
  char name[8];
  uint8_t ipv4_address[4];
  uint8_t ipv4_netmask[4];
  uint8_t ipv4_gateway[4];
  uint8_t mac[6];
  uint32_t iobase;
  uint32_t memory;
  uint8_t irq;
  uint16_t bus, slot, func;
  int link; //link down = 0, up = non-0
  void* custom;
  int (*write)(void*, size_t, uint8_t*, eth_dev_t*); //write int bytes from pointer, send to uint8_t*
  int (*read)(void*, size_t, eth_dev_t*); //read int number of packages to pointer
  int (*available)(eth_dev_t*); //is there anything in the recv buffers
  int (*full)(eth_dev_t*); //are transmit buffers full
  struct {
    int phy_id;
    int advertising;
    int phy_id_mask;
    int reg_num_mask;

    unsigned int full_duplex : 1;
    unsigned int force_media : 1;
    unsigned int supports_gmii : 1;
  } mii;
  //stats
  uint32_t received, sent;
};

typedef struct ethernet_frame {
  uint8_t hwaddr_dest[6];
  uint8_t hwaddr_src[6];
  uint16_t ethertype;
  uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

struct ethernet_device* ethernet_allocate();
void ethernet_free(struct ethernet_device* dev);
void ethernet_list();
int ethernet_send_arp(struct ethernet_device* dev);
const struct ethernet_device* ethernet_getif(int id);
int ethernet_send_packet(struct ethernet_device* dev, void* buf, size_t len, uint8_t* dest, uint16_t protocol);
void ethernet_recv_packet(struct ethernet_device* dev, void* buf, size_t len);
struct ethernet_device* ethernet_getbyname(const char* name);

#endif
