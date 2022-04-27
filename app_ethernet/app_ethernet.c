/**
  ******************************************************************************
  * @file    LwIP/LwIP_TCP_Echo_Client/Src/app_ethernet.c
  * @author  MCD Application Team
  * @brief   Ethernet specefic module
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "main.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif
#include "app_ethernet.h"
#include "ethernetif.h"
#ifdef USE_LCD
#include "lcd_log.h"
#endif
#include "app_debug.h"
#include "stdbool.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

uint32_t EthernetLinkTimer;

#if LWIP_DHCP
#define MAX_DHCP_TRIES  4
uint32_t DHCPfineTimer = 0;
uint8_t DHCP_state = DHCP_OFF;
static bool dhcp_is_start = false;
bool netif_is_removed = false;
#endif

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Notify the User about the network interface config status
  * @param  netif: the network interface
  * @retval None
  */
void ethernet_link_status_updated(struct netif *netif)
{
  if (netif_is_link_up(netif))
  {
#if LWIP_DHCP
    /* Update DHCP state machine */
    DHCP_state = DHCP_START;
    dhcp_is_start = false;
#endif /* LWIP_DHCP */
    DEBUG_INFO("DHCP START STATUS UPDATE\r\n");

  }
  else
  {
#if LWIP_DHCP
    /* Update DHCP state machine */
    DHCP_state = DHCP_LINK_DOWN;
    DEBUG_INFO("DHCP LINK DOWN STATUS UPDATE\r\n");
    Netif_Config(true);
	DEBUG_INFO ("NET CONFIG AGAIN \r\n");

  }
#endif
}

#if LWIP_NETIF_LINK_CALLBACK
/**
  * @brief  Ethernet Link periodic check
  * @param  netif
  * @retval None
  */
void Ethernet_Link_Periodic_Handle(struct netif *netif)
{

  /* Ethernet Link every 100ms */
  if (HAL_GetTick() - EthernetLinkTimer >= 100)
  {
    EthernetLinkTimer = HAL_GetTick();
//    ethernet_link_check_state(netif);
    DEBUG_INFO ("ENTER LINK CALLBACK \r\n");
    ethernet_link_thread(netif);
  }
}

#endif


#if LWIP_DHCP
/**
  * @brief  DHCP Process
  * @param  argument: network interface
  * @retval None
  */
uint8_t iptxt[32];
void DHCP_Thread(void const * argument)
{
  struct netif *netif = (struct netif *) argument;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
  struct dhcp *dhcp;

#ifdef USE_LCD
  uint8_t iptxt[20];
#endif
  DEBUG_INFO("DHCP THREAT START \r\n");
  for (;;)
  {
//	  DEBUG_INFO ("%d", DHCP_state);
//	  if (!DHCP_state)
//	  {
//		  ethernet_link_status_updated(netif);
//	  }
	 // DEBUG_INFO("DHCP STATE : %d\r\n", DHCP_state);
    switch (DHCP_state)
    {
    case DHCP_START:
      {
        ip_addr_set_zero_ip4(&netif->ip_addr);
        ip_addr_set_zero_ip4(&netif->netmask);
        ip_addr_set_zero_ip4(&netif->gw);

        DEBUG_INFO ("LOOKING FOR DHCP SEVER \r\n");
        if (dhcp_is_start != true)
        {
        	DEBUG_INFO ("get in function \r\n");
        	osDelay (1);
        	dhcp_start (netif);
//        	netif_is_removed = false;
        	dhcp_is_start = true;
        	DHCP_state = DHCP_WAIT_ADDRESS;
        	DEBUG_INFO ("DHCP START \r\n");
        }
      }
      break;
    case DHCP_WAIT_ADDRESS:
      {
    	  DEBUG_INFO ("WAITING DHCP SEVER \r\n");
        if (dhcp_supplied_address(netif))
        {
          DHCP_state = DHCP_ADDRESS_ASSIGNED;

#ifdef USE_LCD
          sprintf((char *)iptxt, "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
          LCD_UsrLog ("IP address assigned by a DHCP server: %s\n", iptxt);
#else
//          BSP_LED_On(LED1);
//          BSP_LED_Off(LED2);

#endif
          sprintf((char *)iptxt, "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
          DEBUG_INFO("IP address assigned by a DHCP server\r\n");
          DEBUG_INFO("%s\r\n", iptxt);
          DEBUG_INFO("Done\r\n");


        }
        else
        {
          dhcp = (struct dhcp *)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

          /* DHCP timeout */
          if (dhcp->tries > MAX_DHCP_TRIES)
          {
            DHCP_state = DHCP_TIMEOUT;
            dhcp_stop (netif);
            dhcp_is_start = false;
            /* Static address used */
            IP_ADDR4(&ipaddr, IP_ADDR0 ,IP_ADDR1 , IP_ADDR2 , IP_ADDR3 );
            IP_ADDR4(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
            IP_ADDR4(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
            netif_set_addr(netif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), ip_2_ip4(&gw));

            sprintf((char *)iptxt, "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
            DEBUG_ERROR ("DHCP Timeout !! \n");
            DEBUG_INFO ("Static IP address: %s\n", iptxt);

          }
        }
      }
      break;
    case DHCP_ADDRESS_ASSIGNED:
    {
    	xSemaphoreGive(hHttpStart);
    }
    break;
    case DHCP_TIMEOUT:
    {
    	xSemaphoreGive(hHttpStart);
//    	DHCP_state = DHCP_START;
    }
    	break;
    case DHCP_LINK_DOWN:
    {
      DHCP_state = DHCP_OFF;
      if (dhcp_is_start == true)
      {
    	  dhcp_stop (netif);
    	  dhcp_is_start = false;
    	  DEBUG_INFO ("DHCP STOPPED \r\n");
      }
      DEBUG_INFO ("DHCPLINK DOWN\r\n");
    }
    break;
    default: break;
    }

    /* wait 500 ms */
    osDelay(500);
  }
}
#endif  /* LWIP_DHCP */
/**
  * @brief  DHCP periodic check
  * @param  netif
  * @retval None
  */
//void DHCP_Periodic_Handle(struct netif *netif)
//{
//
//  /* Fine DHCP periodic process every 500ms */
//  if (HAL_GetTick() - DHCPfineTimer >= DHCP_FINE_TIMER_MSECS)
//  {
////	  DEBUG_INFO("GET INTO DHCP HANDLE\r\n");
//	  DHCPfineTimer =  HAL_GetTick();
//    /* process DHCP state machine */
//    //DHCP_Process(netif);
//  }
//}

