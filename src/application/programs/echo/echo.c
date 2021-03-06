/*=========================================================================*//**
@file    echo.c

@author  Daniel Zorychta

@brief   Print messages on terminal

@note    Copyright (C) 2015 Daniel Zorychta <daniel.zorychta@gmail.com>

         This program is free software; you can redistribute it and/or modify
         it under the terms of the GNU General Public License as published by
         the Free Software Foundation and modified by the dnx RTOS exception.

         NOTE: The modification  to the GPL is  included to allow you to
               distribute a combined work that includes dnx RTOS without
               being obliged to provide the source  code for proprietary
               components outside of the dnx RTOS.

         The dnx RTOS  is  distributed  in the hope  that  it will be useful,
         but WITHOUT  ANY  WARRANTY;  without  even  the implied  warranty of
         MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the
         GNU General Public License for more details.

         Full license text is available on the following file: doc/license.txt.


*//*==========================================================================*/

/*==============================================================================
  Include files
==============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*==============================================================================
  Local symbolic constants/macros
==============================================================================*/

/*==============================================================================
  Local types, enums definitions
==============================================================================*/

/*==============================================================================
  Local function prototypes
==============================================================================*/

/*==============================================================================
  Local object definitions
==============================================================================*/
GLOBAL_VARIABLES_SECTION {
};

/*==============================================================================
  Exported object definitions
==============================================================================*/

/*==============================================================================
  Function definitions
==============================================================================*/
//==============================================================================
/**
 * @brief Cat main function
 */
//==============================================================================
int_main(echo, STACK_DEPTH_LOW, int argc, char *argv[])
{
        int err = 0;

        for (int i = 1; i < argc; i++) {
                if (i == argc - 1) {
                        printf("%s", argv[i]);
                } else {
                        printf("%s ", argv[i]);
                }

                if (ferror(stdout)) {
                        err = errno;
                        break;
                }
        }

        putchar('\n');

        if (err) {
                fprintf(stderr, "echo: %s\n", strerror(err));
        }

        return EXIT_SUCCESS;
}

/*==============================================================================
  End of file
==============================================================================*/
