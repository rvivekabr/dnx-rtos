#ifndef ETHER_CFG_H_
#define ETHER_CFG_H_
/*=============================================================================================*//**
@file    ether_cfg.h

@author  Daniel Zorychta

@brief

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


/*==================================================================================================
                                 Exported symbolic constants/macros
==================================================================================================*/
#define ETHER_PHY_ADDRESS                 0x01

#define ETH_PART_NONE                     0

/** port names */
enum ETH_DEV_NUMBER_enum
{
      ETH_DEV_1,
      ETH_DEV_LAST
};


/*==================================================================================================
                                  Exported types, enums definitions
==================================================================================================*/


/*==================================================================================================
                                     Exported object declarations
==================================================================================================*/


/*==================================================================================================
                                     Exported function prototypes
==================================================================================================*/


#ifdef __cplusplus
   }
#endif

#endif /* ETHER_CFG_H_ */
/*==================================================================================================
                                            End of file
==================================================================================================*/
