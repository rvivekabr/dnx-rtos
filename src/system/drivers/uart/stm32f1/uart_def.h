#ifndef _UART_DEF_H_
#define _UART_DEF_H_
/*=========================================================================*//**
@file    usart_def.h

@author  Daniel Zorychta

@brief   This file support statuses and request of USART peripherals

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


*//*==========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
  Include files
==============================================================================*/
#include "core/systypes.h"

/*==============================================================================
  Exported symbolic constants/macros
==============================================================================*/

/*==============================================================================
  Exported types, enums definitions
==============================================================================*/
/** IO request for UART driver */
enum UART_IORq
{
        UART_IORQ_ENABLE_WAKEUP_IDLE,                   /* [---] no arg       */
        UART_IORQ_ENABLE_WAKEUP_ADDRESS_MARK,           /* [---] no arg       */
        UART_IORQ_ENABLE_PARITY_CHECK,                  /* [---] no arg       */
        UART_IORQ_DISABLE_PARITY_CHECK,                 /* [---] no arg       */
        UART_IORQ_SET_ODD_PARITY,                       /* [---] no arg       */
        UART_IORQ_SET_EVEN_PARITY,                      /* [---] no arg       */
        UART_IORQ_ENABLE_RECEIVER_WAKEUP_MUTE,          /* [---] no arg       */
        UART_IORQ_DISABLE_RECEIVER_WAKEUP_MUTE,         /* [---] no arg       */
        UART_IORQ_ENABLE_LIN_MODE,                      /* [---] no arg       */
        UART_IORQ_DISABLE_LIN_MODE,                     /* [---] no arg       */
        UART_IORQ_SET_1_STOP_BIT,                       /* [---] no arg       */
        UART_IORQ_SET_2_STOP_BITS,                      /* [---] no arg       */
        UART_IORQ_SET_LIN_BRK_DETECTOR_11_BITS,         /* [---] no arg       */
        UART_IORQ_SET_LIN_BRK_DETECTOR_10_BITS,         /* [---] no arg       */
        UART_IORQ_SET_ADDRESS_NODE,                     /* [in ] int          */
        UART_IORQ_ENABLE_CTS,                           /* [---] no arg       */
        UART_IORQ_DISABLE_CTS,                          /* [---] no arg       */
        UART_IORQ_ENABLE_RTS,                           /* [---] no arg       */
        UART_IORQ_DISABLE_RTS,                          /* [---] no arg       */
        UART_IORQ_GET_BYTE,                             /* [out] u8_t*        */
        UART_IORQ_GET_BYTE_BLOCKING,                    /* [out] u8_t*        */
        UART_IORQ_SEND_BYTE,                            /* [in ] int          */
        UART_IORQ_SET_BAUDRATE,                         /* [in ] int          */
};

/*==============================================================================
  Exported object declarations
==============================================================================*/

/*==============================================================================
 Exported function prototypes
==============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* _UART_DEF_H_ */
/*==============================================================================
  End of file
==============================================================================*/