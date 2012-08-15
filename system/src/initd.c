/*=============================================================================================*//**
@file    initd.c

@author  Daniel Zorychta

@brief   This file contain initialize and runtime daemon

@note    Copyright (C) 2012 Daniel Zorychta <daniel.zorychta@gmail.com>

         This program is free software; you can redistribute it and/or modify
         it under the terms of the GNU General Public License as published by
         the  Free Software  Foundation;  either version 2 of the License, or
         any later version.

         This  program  is  distributed  in the hope that  it will be useful,
         but  WITHOUT  ANY  WARRANTY;  without  even  the implied warranty of
         MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the
         GNU General Public License for more details.

         You  should  have received a copy  of the GNU General Public License
         along  with  this  program;  if not,  write  to  the  Free  Software
         Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


*//*==============================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
                                            Include files
==================================================================================================*/
#include "initd.h"
#include "uart.h"
#include "ether.h"
#include "netconf.h"
#include "ds1307.h"

#include "lwiptest.h"
#include "httpde.h"


/*==================================================================================================
                                  Local symbolic constants/macros
==================================================================================================*/


/*==================================================================================================
                                   Local types, enums definitions
==================================================================================================*/


/*==================================================================================================
                                      Local function prototypes
==================================================================================================*/


/*==================================================================================================
                                      Local object definitions
==================================================================================================*/


/*==================================================================================================
                                     Exported object definitions
==================================================================================================*/


/*==================================================================================================
                                        Function definitions
==================================================================================================*/

//================================================================================================//
/**
 * @brief Task which initialise high-level devices/applications etc
 * Task is responsible for low-level application runtime environment (stdio). Task connect
 * applications' stdios with hardware layer.
 */
//================================================================================================//
void Initd(void *arg)
{
      (void) arg;

      /* short delay and lock task scheduling */
      TaskDelay(1000);

      /*--------------------------------------------------------------------------------------------
       * initialization kprint()
       *------------------------------------------------------------------------------------------*/
      UART_Open(UART_DEV_1);
      kprintEnable();

      /* VT100 terminal configuration */
      clrscr(k);
      enableLineWrap(k);

      /* something about board and system */
      kprint("Board powered by "); fontGreen(k); kprint("FreeRTOS\n"); resetAttr(k);

      kprint("By "); fontCyan(k); kprint("Daniel Zorychta ");
      fontYellow(k); kprint("<daniel.zorychta@gmail.com>\n\n"); resetAttr(k);
      TaskDelay(1000);

      /* info about system start */
      kprint("[%d] initd: kernel print started\n", TaskGetTickCount());
      kprint("[%d] initd: init daemon started\n", TaskGetTickCount());

      /*--------------------------------------------------------------------------------------------
       * user initialization
       *------------------------------------------------------------------------------------------*/
      if (ETHER_Init(ETH_DEV_1) != STD_RET_OK)
            goto initd_net_end;

      if (LwIP_Init() != STD_RET_OK)
            goto initd_net_end;

//      kprint("Starting telnetd... ");
//      if (TaskCreate(telnetd, "telnetd", TELNETD_STACK_SIZE, NULL, 2, NULL) == pdPASS)
//      {
//            fontGreen(k);
//            kprint("SUCCESS\n");
//      }
//      else
//      {
//            fontRed(k);
//            kprint("FAILED\n");
//      }
//      resetAttr(k);

      kprint("Starting httpde... ");
      if (TaskCreate(httpd_init, "httpde", HTTPDE_STACK_SIZE, NULL, 2, NULL) == pdPASS)
      {
            fontGreen(k);
            kprint("SUCCESS\n");
      }
      else
      {
            fontRed(k);
            kprint("FAILED\n");
      }
      resetAttr(k);

      initd_net_end:

      DS1307_Init();

      /*--------------------------------------------------------------------------------------------
       * starting terminal
       *------------------------------------------------------------------------------------------*/
      kprint("[%d] initd: starting interactive console... ", TaskGetTickCount());

      /* try to start terminal */
      stdRet_t status;
      appArgs_t *appHdl = Exec("terminal", NULL, &status);

      if (status != STD_RET_OK)
      {
            fontRed(k); kprint("FAILED\n"); resetAttr(k);
            kprint("Probably no enough free space. Restarting board...");
            TaskResumeAll();
            TaskDelay(5000);
            SystemReboot();
      }
      else
      {
            fontGreen(k); kprint("SUCCESS\n"); resetAttr(k);
      }

      /* initd info about stack usage */
      kprint("[%d] initd: free stack: %d\n\n", TaskGetTickCount(), TaskGetStackFreeSpace(THIS_TASK));

      /*--------------------------------------------------------------------------------------------
       * main loop which read stdios from applications
       *------------------------------------------------------------------------------------------*/
      for (;;)
      {
            ch_t   character;
            bool_t stdoutEmpty = FALSE;
            bool_t RxFIFOEmpty = FALSE;

            /* STDOUT support ------------------------------------------------------------------- */
            if ((character = ufgetChar(appHdl->stdout)) != ASCII_CANCEL)
            {
                  UART_IOCtl(UART_DEV_1, UART_IORQ_SEND_BYTE, &character);
                  stdoutEmpty = FALSE;
            }
            else
            {
                  stdoutEmpty = TRUE;
            }

            /* STDIN support -------------------------------------------------------------------- */
            if (UART_IOCtl(UART_DEV_1, UART_IORQ_GET_BYTE, &character) == STD_RET_OK)
            {
                  ufputChar(appHdl->stdin, character);
                  RxFIFOEmpty = FALSE;
            }
            else
            {
                  RxFIFOEmpty = TRUE;
            }

            /* application monitoring ----------------------------------------------------------- */
            if (appHdl->exitCode != STD_RET_UNKNOWN)
            {
                  if (appHdl->exitCode == STD_RET_OK)
                        kprint("\n[%d] initd: terminal was terminated.\n", TaskGetTickCount());
                  else
                        kprint("\n[%d] initd: terminal was terminated with error.\n", TaskGetTickCount());

                  FreeAppStdio(appHdl);

                  kprint("[%d] initd: disable FreeRTOS scheduler. Bye.\n", TaskGetTickCount());

                  vTaskEndScheduler();

                  while (TRUE)
                        TaskDelay(1000);
            }

            /* wait state */
            if (stdoutEmpty && RxFIFOEmpty)
                  TaskDelay(1);
      }

      /* this should never happen */
      TaskTerminate();
}


#ifdef __cplusplus
}
#endif

/*==================================================================================================
                                            End of file
==================================================================================================*/
