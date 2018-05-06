#include "network.h"
#include "../Marlin.h"
#include "uip/uip.h"
#include "uip/uip_arp.h"
#include <lpc17xx_emac.h>
#include "clock-arch.h"
#include "telnetd/telnetd.h"

static int network_debug = 0;

#define EMAC_MAX_PACKET_SIZE (UIP_CONF_BUFFER_SIZE + 16)    // 1536 bytes

static int32_t read_PHY (uint32_t PhyReg)
{
	/* Read a PHY register 'PhyReg'. */
	uint32_t tout;

	LPC_EMAC->MADR = EMAC_DEF_ADR | PhyReg;
	LPC_EMAC->MCMD = EMAC_MCMD_READ;

	/* Wait until operation completed */
	tout = 0;
	for (tout = 0; tout < EMAC_MII_RD_TOUT; tout++) {
		if ((LPC_EMAC->MIND & EMAC_MIND_BUSY) == 0) {
			LPC_EMAC->MCMD = 0;
			return (LPC_EMAC->MRDD);
		}
	}
	// Time out!
	return (-1);
}

extern "C" void uip_lprintf(const char *fmt, ...)
{
  char x[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(x, sizeof(x), fmt, ap);
  va_end(ap);
  uip_log(x);
}

UNS_32 tapdev_read(void * pPacket)
{
  UNS_32 Size = EMAC_MAX_PACKET_SIZE;
  UNS_32 in_size;
  EMAC_PACKETBUF_Type RxPack;

  // Check Receive status
  if (!EMAC_CheckReceiveIndex()){
      return (0);
  }

  // Get size of receive data
  in_size = EMAC_GetReceiveDataSize() + 1;
  if (in_size > Size)
    uip_lprintf("read too large packet %u", in_size);

  Size = MIN(Size,in_size);

  // Setup Rx packet
  RxPack.pbDataBuf = (uint32_t *)pPacket;
  RxPack.ulDataLen = Size;
  EMAC_ReadPacketBuffer(&RxPack);

  // update receive status
  EMAC_UpdateRxConsumeIndex();
  return(Size);
}

/* transmit an Ethernet frame to MAC/DMA controller */
BOOL_8 tapdev_send(void *pPacket, UNS_32 size)
{
  EMAC_PACKETBUF_Type TxPack;

  // Check size
  if(size == 0){
      return(TRUE);
  }

  // check Tx Slot is available
  if (!EMAC_CheckTransmitIndex()){
      return (FALSE);
  }

  uip_lprintf("write packet len %u pPacket %p", size, pPacket);

  size = MIN(size,EMAC_MAX_PACKET_SIZE);

  // Setup Tx Packet buffer
  TxPack.ulDataLen = size;
  TxPack.pbDataBuf = (uint32_t *)pPacket;
  EMAC_WritePacketBuffer(&TxPack);
  EMAC_UpdateTxProduceIndex();

  return(TRUE);
}

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

static   struct timer periodic_timer, arp_timer, check_timer;

void network_loop()
{
  uip_len = tapdev_read(uip_buf);
  if(uip_len > 0)
  {
    if (network_debug) {
      uip_lprintf("read something %d from %02x:%02x:%02x:%02x:%02x:%02x", ntohs(BUF->type), BUF->src.addr[0], BUF->src.addr[1], BUF->src.addr[2], BUF->src.addr[3], BUF->src.addr[4], BUF->src.addr[5]);
    }
    if(BUF->type == htons(UIP_ETHTYPE_IP))
    {
      uip_arp_ipin();
      uip_input();
      /* If the above function invocation resulted in data that
          should be sent out on the network, the global variable
          uip_len is set to a value > 0. */

      if(uip_len > 0)
      {
          uip_arp_out();
          tapdev_send(uip_buf,uip_len);
      }
    }
    else if(BUF->type == htons(UIP_ETHTYPE_ARP))
    {
      uip_arp_arpin();
      /* If the above function invocation resulted in data that
          should be sent out on the network, the global variable
          uip_len is set to a value > 0. */
      if(uip_len > 0)
      {
          tapdev_send(uip_buf,uip_len);
      }
    }
  }
  else if(timer_expired(&periodic_timer))
  {
    timer_reset(&periodic_timer);
    for(int i = 0; i < UIP_CONNS; i++)
    {
      uip_periodic(i);
      /* If the above function invocation resulted in data that
          should be sent out on the network, the global variable
          uip_len is set to a value > 0. */
      if(uip_len > 0)
      {
        uip_arp_out();
        tapdev_send(uip_buf,uip_len);
      }
    }
#if UIP_UDP
    for(int i = 0; i < UIP_UDP_CONNS; i++) {
      uip_udp_periodic(i);
      /* If the above function invocation resulted in data that
          should be sent out on the network, the global variable
          uip_len is set to a value > 0. */
      if(uip_len > 0) {
        uip_arp_out();
        tapdev_send(uip_buf, uip_len);
      }
    }
#endif /* UIP_UDP */
    /* Call the ARP timer function every 10 seconds. */
    if(timer_expired(&arp_timer))
    {
      timer_reset(&arp_timer);
      uip_arp_timer();
    }
  }
}

static bool init = false;

void network_test()
{
  SERIAL_ECHOPGM("Some network debug ");
  SERIAL_ECHOLN(freeMemory());
  if (!init)
  {
    init = true;
    SERIAL_ECHOPGM("uip len ");
    SERIAL_ECHOLN(uip_len);
    EMAC_CFG_Type Emac_Config;
    /* pin configuration */
    PINSEL_CFG_Type PinCfg;

    PinCfg.Funcnum = 1;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 1;

    PinCfg.Pinnum = 0;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 1;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 4;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 8;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 9;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 10;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 14;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 15;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 16;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 17;
    PINSEL_ConfigPin(&PinCfg);  
    
    Emac_Config.Mode = EMAC_MODE_AUTO;
    Emac_Config.pbEMAC_Addr = uip_ethaddr.addr;
    // Initialize EMAC module with given parameter
    int err = EMAC_Init(&Emac_Config);
    SERIAL_ECHOPGM("eth ");
    SERIAL_ECHOLN(err);

    HAL_timer_start(NETWORK_TIMER_NUM, 100);
    HAL_timer_enable_interrupt(NETWORK_TIMER_NUM);

    uip_lprintf("speed %d dup %d", EMAC_CheckPHYStatus(EMAC_PHY_STAT_SPEED), EMAC_CheckPHYStatus(EMAC_PHY_STAT_DUP));
    dhcpc_init(uip_ethaddr.addr, sizeof(uip_ethaddr.addr)); //, "dbot");
    uip_lprintf("dhcpc init %02x:%02x:%02x:%02x:%02x:%02x", uip_ethaddr.addr[0], uip_ethaddr.addr[1], uip_ethaddr.addr[2], uip_ethaddr.addr[3], uip_ethaddr.addr[4], uip_ethaddr.addr[5]);
    dhcpc_request();
  }

  //network_loop();
}

static bool isUp;

void network_idle()
{
  if (!init)
    return;

  if (!isUp)
  {
    if (!timer_expired(&check_timer))
      return;

    int32_t st = read_PHY(EMAC_PHY_REG_BMSR);
    int32_t stat = EMAC_CheckPHYStatus(EMAC_PHY_STAT_LINK);
    uip_lprintf("status %d full %x ad %d", stat, st, (st & EMAC_PHY_BMSR_AUTO_DONE) != 0);
    if (stat)
      isUp = true;
    else {
      timer_reset(&check_timer);
      return;
    }
  }

  network_loop();
}

void uip_log(const char *msg)
{
  SERIAL_ECHOLN(msg);
}

HAL_NETWORK_TIMER_ISR {
  HAL_timer_isr_prologue(NETWORK_TIMER_NUM);
  clock_tick();
  HAL_timer_isr_epilogue(NETWORK_TIMER_NUM);
}

void network_init()
{
  uip_log("network init");

  timer_set(&periodic_timer, CLOCK_SECOND / 2); /*0.5s */
  timer_set(&arp_timer, CLOCK_SECOND * 10);   /*10s */
  timer_set(&check_timer, CLOCK_SECOND);   /*1s */

  uip_init();
  // 8D-BE-B1-37-98-7E
  // random MAC address from an online generator
  uip_eth_addr mac = { 0x82, 0xbe, 0xb1, 0x37, 0x98, 0x7e };
  uip_setethaddr(mac);
}

extern "C" void dhcpc_configured(const dhcpc_state *s)
{
  uip_lprintf("IP address %d.%d.%d.%d\n", uip_ipaddr1(s->ipaddr), uip_ipaddr2(s->ipaddr), uip_ipaddr3(s->ipaddr), uip_ipaddr4(s->ipaddr));
  uip_lprintf("netmask %d.%d.%d.%d\n", uip_ipaddr1(s->netmask), uip_ipaddr2(s->netmask), uip_ipaddr3(s->netmask), uip_ipaddr4(s->netmask));
  uip_lprintf("DNS server %d.%d.%d.%d\n", uip_ipaddr1(s->dnsaddr), uip_ipaddr2(s->dnsaddr), uip_ipaddr3(s->dnsaddr), uip_ipaddr4(s->dnsaddr));
  uip_lprintf("Default router %d.%d.%d.%d\n", uip_ipaddr1(s->default_router), uip_ipaddr2(s->default_router), uip_ipaddr3(s->default_router), uip_ipaddr4(s->default_router));
  uip_lprintf("Lease expires in %ld seconds\n", ntohs(s->lease_time[0])*65536ul + ntohs(s->lease_time[1]));

  uip_sethostaddr(s->ipaddr);
  uip_setnetmask(s->netmask);
  uip_setdraddr(s->default_router);

  telnetd_init();
}

extern "C" void ENET_IRQHandler()
{
}

void network_set_debug(int flag)
{
  network_debug = flag;
  uip_lprintf("Network debug is now %s", flag ? "on" : "off");
}