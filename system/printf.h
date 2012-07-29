#ifndef PRINTF_H_
#define PRINTF_H_
/*=============================================================================================*//**
@file    printf.h

@author  Daniel Zorychta

@brief   This file support message printing

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
#include <stdarg.h>
#include "system.h"


/*==================================================================================================
                                 Exported symbolic constants/macros
==================================================================================================*/
#define print(format, __var_args)                sprint(stdio, format, __var_args)


/*==================================================================================================
                                  Exported types, enums definitions
==================================================================================================*/


/*==================================================================================================
                                     Exported object declarations
==================================================================================================*/


/*==================================================================================================
                                     Exported function prototypes
==================================================================================================*/
extern ch_t  *itoa(i32_t value, ch_t *buffer, u8_t base);
extern u32_t bprint(ch_t *stream, u32_t size, const ch_t *format, ...);
extern u32_t kprint(const ch_t *format, ...);
extern u32_t sprint(stdio_t *stdio, const ch_t *format, ...);
extern void  kprintEnable(void);
extern void  PutChar(stdio_t *stdio, ch_t c);
extern ch_t  GetChar(stdio_t *stdio);
extern void  ClearStdin(stdio_t *stdio);
extern void  ClearStdout(stdio_t *stdio);


#ifdef __cplusplus
   }
#endif

#endif /* PRINTF_H_ */
/*==================================================================================================
                                            End of file
==================================================================================================*/