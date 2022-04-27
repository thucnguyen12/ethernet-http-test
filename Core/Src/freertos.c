/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "app_ethernet.h"
#include "app_debug.h"
#include "lwip.h"
#include "stdio.h"
#include "app_http.h"
#include "lwip/dns.h"
#include "mqtt_client.h"
#include "stdbool.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
struct netif g_netif;
//static bool task_created = false;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
static TaskHandle_t m_task_handle_http = NULL;
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void Netif_Config (bool restart);
void http_task(void *argument);
static void dns_initialize(void);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
	  hHttpStart = xSemaphoreCreateBinary();
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 1024);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for LWIP */
  //MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */
  tcpip_init( NULL, NULL );
  Netif_Config (false);
  dns_initialize();
  if (m_task_handle_http == NULL)
  {
	  xTaskCreate(http_task, "http_task", 4096, NULL, 0, &m_task_handle_http);
  }
	DEBUG_INFO("DHCP TASK CREATED\r\n");

	// dhcp thread

	  /* Create the Ethernet link handler thread */
	/* USER CODE BEGIN H7_OS_THREAD_DEF_CREATE_CMSIS_RTOS_V1 */
	  osThreadDef(EthLink, ethernet_link_thread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE);
	  osThreadCreate (osThread(EthLink), &g_netif);

	/* USER CODE END H7_OS_THREAD_DEF_CREATE_CMSIS_RTOS_V1 */

	  /* Start DHCP negotiation for a network interface (IPv4) */
#if LWIP_DHCP
  /* Start DHCPClient */
  osThreadDef(DHCP, DHCP_Thread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE * 2);
  osThreadCreate (osThread(DHCP), &g_netif);
#endif

  /* Infinite loop */
  for(;;)
  {
//		app_debug_isr_ringbuffer_flush();
	  vTaskDelete(defaultTaskHandle);

//	osThreadTerminate(NULL);
//	DEBUG_INFO ("TASK TERMINATED \r\n");

  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void http_task(void *argument)
{
	DEBUG_INFO("ENTER THE HTTP AND MQTT TASK\r\n");
	xSemaphoreTake(hHttpStart, portMAX_DELAY);
	DEBUG_INFO ("PASS SEM TAKE \r\n");
//	m_http_test_started = true;


#if 0
	// http://httpbin.org/get
	app_http_config_t http_cfg;
	sprintf(http_cfg.url, "%s", "httpbin.org");
	http_cfg.port = 80;
	sprintf(http_cfg.file, "%s", "/get");
	http_cfg.on_event_cb = (void*)0;
	app_http_start(&http_cfg);
#else
    mqtt_client_cfg_t mqtt_cfg =
    {
        .periodic_sub_req_s = 120,            // second
        .broker_addr = "broker.hivemq.com",
        .port = 1883,
        .password = NULL,
        .client_id = "test_lwip_porting",
    };
    mqtt_client_initialize(&mqtt_cfg);
	for(;;)
	{
		xSemaphoreTake(hHttpStart, portMAX_DELAY);
		mqtt_client_polling_task(NULL);
		osDelay(1000);

	}
#endif
//	for(;;)
//	{
//
//		osDelay(1000);
//
//	}
}
void Netif_Config (bool restart)
{
	ip4_addr_t ipaddr;
	ip4_addr_t netmask;
	ip4_addr_t gw;
	  /* IP addresses initialization with DHCP (IPv4) */
	  ipaddr.addr = 0;
	  netmask.addr = 0;
	  gw.addr = 0;
	  if (restart)
	  {
			netif_remove (&g_netif);
			DEBUG_INFO ("NET IF REMOVE \r\n");
			if (HAL_ETH_DeInit (&heth) == HAL_OK)
			{
				DEBUG_INFO ("DEINIT ETH \r\n");
			}
	  }
	  /* add the network interface (IPv4/IPv6) with RTOS */
	  netif_add(&g_netif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);//&tcpip_input =>null

	  /* Registers the default network interface */
	  netif_set_default(&g_netif);

	  if (netif_is_link_up(&g_netif))
	  {
	    /* When the netif is fully configured this function must be called */
	    netif_set_up(&g_netif);
	  }
	  else
	  {
	    /* When the netif link is down this function must be called */
	    netif_set_down(&g_netif);
	  }

	  /* Set the link callback function, this function is called on change of link status*/
	  ethernet_link_status_updated(&g_netif);
	  netif_set_link_callback(&g_netif, ethernet_link_status_updated);
	  DEBUG_INFO ("SET LINK CALLBACK \r\n");
}

static void dns_initialize(void)
{
    ip_addr_t dns_server_0 = IPADDR4_INIT_BYTES(8, 8, 8, 8);
    ip_addr_t dns_server_1 = IPADDR4_INIT_BYTES(1, 1, 1, 1);
    dns_setserver(0, &dns_server_0);
    dns_setserver(1, &dns_server_1);
    dns_init();
}
/* USER CODE END Application */
