/*=========================================================================*//**
@file    ioctl_macros.h

@author  Daniel Zorychta

@brief   ioctl build macros.

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

#ifndef _IOCTL_MACROS_H_
#define _IOCTL_MACROS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
  Include files
==============================================================================*/
#include "ioctl_groups.h"

/*==============================================================================
  Exported macros
==============================================================================*/
#define _IO(g, n)               (((u32_t)(_IO_GROUP_##g) << 16) | ((u32_t)(n) & 0xFFFF))
#define _IOR(g, n, t)           (((u32_t)(_IO_GROUP_##g) << 16) | ((u32_t)(n) & 0xFFFF))
#define _IOW(g, n, t)           (((u32_t)(_IO_GROUP_##g) << 16) | ((u32_t)(n) & 0xFFFF))
#define _IOWR(g, n, t)          (((u32_t)(_IO_GROUP_##g) << 16) | ((u32_t)(n) & 0xFFFF))

/* predefined requests*/
/**
 *  @brief  Initialize storage device
 *  @param  None
 *  @return On success (device initialized) 0 is returned.
 *          On error (device not initialized) -1 is returned and errno is set.
 */
#define IOCTL_STORAGE__INITIALIZE       _IO(STORAGE, 0x00)

/**
 *  @brief  Read storage's MBR sector and detect partitions
 *  @param  None
 *  @return On success (MBR detected) 0 is returned.
 *          On error (MBR not exist or IO error) -1 is returned and errno is set.
 */
#define IOCTL_STORAGE__READ_MBR         _IO(STORAGE, 0x01)

/*==============================================================================
  Exported object types
==============================================================================*/

/*==============================================================================
  Exported objects
==============================================================================*/

/*==============================================================================
  Exported functions
==============================================================================*/

/*==============================================================================
  Exported inline functions
==============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* _IOCTL_MACROS_H_ */
/*==============================================================================
  End of file
==============================================================================*/