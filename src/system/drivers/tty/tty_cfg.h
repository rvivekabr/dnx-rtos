#ifndef _TTY_CFG_H_
#define _TTY_CFG_H_
/*=========================================================================*//**
@file    tty_cfg.h

@author  Daniel Zorychta

@brief   This file support configuration of TTY

@note    Copyright (C) 2013 Daniel Zorychta <daniel.zorychta@gmail.com>

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

/*==============================================================================
  Exported macros
==============================================================================*/
/** define TTY max supported lines */
#define TTY_MAX_LINES               40

/** define output stream size (from keyboard) */
#define TTY_STREAM_SIZE             80

/** define edit line length */
#define TTY_EDIT_LINE_LEN           80

/** define number of virtual terminals */
#define TTY_NUMBER_OF_VT            4

/** enable (1) or disable (0) checking terminal size */
#define TTY_TERM_SIZE_CHECK         1

/*==============================================================================
  Exported object types
==============================================================================*/
/** devices number */
enum tty_list {
#if TTY_NUMBER_OF_VT > 0
        TTY_DEV_0,
#endif
#if TTY_NUMBER_OF_VT > 1
        TTY_DEV_1,
#endif
#if TTY_NUMBER_OF_VT > 2
        TTY_DEV_2,
#endif
#if TTY_NUMBER_OF_VT > 3
        TTY_DEV_3,
#endif
#if TTY_NUMBER_OF_VT > 4
#error "TTY support 4 virtual terminals!"
#endif
        TTY_DEV_COUNT
};

/*==============================================================================
  Exported objects
==============================================================================*/

/*==============================================================================
  Exported functions
==============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* _TTY_CFG_H_ */
/*==============================================================================
  End of file
==============================================================================*/