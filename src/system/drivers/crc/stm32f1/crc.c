/*=========================================================================*//**
@file    crc.c

@author  Daniel Zorychta

@brief   CRC driver (CRCCU - CRC Calculation Unit)

@note    Copyright (C) 2014 Daniel Zorychta <daniel.zorychta@gmail.com>

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

/*==============================================================================
  Include files
==============================================================================*/
#include <dnx/module.h>
#include <dnx/thread.h>
#include "stm32f1/crc_cfg.h"
#include "stm32f1/crc_def.h"
#include "stm32f1/stm32f10x.h"

/*==============================================================================
  Local macros
==============================================================================*/

/*==============================================================================
  Local object types
==============================================================================*/
typedef struct CRCCU
{
        dev_lock_t file_lock;
} CRCCU;

/*==============================================================================
  Local function prototypes
==============================================================================*/
static inline void reset_CRC();

/*==============================================================================
  Local objects
==============================================================================*/

/*==============================================================================
  Exported objects
==============================================================================*/

/*==============================================================================
  Function definitions
==============================================================================*/

//==============================================================================
/**
 * @brief Initialize device
 *
 * @param[out]          **device_handle        device allocated memory
 * @param[in ]            major                major device number
 * @param[in ]            minor                minor device number
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_INIT(CRCCU, void **device_handle, u8_t major, u8_t minor)
{
        STOP_IF(!device_handle);

        if (major != _CRC_MAJOR_NUMBER || minor != _CRC_MINOR_NUMBER)
                return STD_RET_ERROR;

        CRCCU *hdl = calloc(1, sizeof(CRCCU));
        if (hdl) {
                SET_BIT(RCC->AHBENR, RCC_AHBENR_CRCEN);

                *device_handle = hdl;
                return STD_RET_OK;
        } else {
                return STD_RET_ERROR;
        }
}

//==============================================================================
/**
 * @brief Release device
 *
 * @param[in ]          *device_handle          device allocated memory
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_RELEASE(CRCCU, void *device_handle)
{
        STOP_IF(device_handle);

        CRCCU *hdl = device_handle;

        critical_section_begin();

        stdret_t status = STD_RET_ERROR;

        if (device_is_unlocked(hdl->file_lock)) {
                CLEAR_BIT(RCC->AHBENR, RCC_AHBENR_CRCEN);
                free(hdl);
                status = STD_RET_OK;
        } else {
                errno = EBUSY;
        }

        critical_section_end();

        return status;
}

//==============================================================================
/**
 * @brief Open device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]           flags                  file operation flags (O_RDONLY, O_WRONLY, O_RDWR)
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_OPEN(CRCCU, void *device_handle, int flags)
{
        STOP_IF(!device_handle);
        UNUSED_ARG(flags);

        CRCCU *hdl = device_handle;

        return device_lock(&hdl->file_lock) ? STD_RET_OK : STD_RET_ERROR;
}

//==============================================================================
/**
 * @brief Close device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]           force                  device force close (true)
 * @param[in ]          *opened_by_task         task with opened this device (valid only if force is true)
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_CLOSE(CRCCU, void *device_handle, bool force, const task_t *opened_by_task)
{
        STOP_IF(!device_handle);
        UNUSED_ARG(opened_by_task);

        CRCCU *hdl = device_handle;

        if (device_is_access_granted(&hdl->file_lock)) {
                device_unlock(&hdl->file_lock, force);
                return STD_RET_OK;
        } else {
                if (force)
                        device_unlock(&hdl->file_lock, force);

                return STD_RET_ERROR;
        }
}

//==============================================================================
/**
 * @brief Write data to device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]          *src                    data source
 * @param[in ]           count                  number of bytes to write
 * @param[in ][out]     *fpos                   file position
 *
 * @return number of written bytes, -1 if error
 */
//==============================================================================
API_MOD_WRITE(CRCCU, void *device_handle, const u8_t *src, size_t count, u64_t *fpos)
{
        UNUSED_ARG(fpos);

        STOP_IF(device_handle == NULL);
        STOP_IF(src == NULL);
        STOP_IF(count == 0);

        CRCCU *hdl = device_handle;

        ssize_t n = -1;

        if (device_is_access_granted(&hdl->file_lock)) {
                reset_CRC();

                for (n = 0; n < (ssize_t)count; n++) {
                        CRC->DR = src[n];
                }
        } else {
                errno = EACCES;
        }

        return n;
}

//==============================================================================
/**
 * @brief Read data from device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[out]          *dst                    data destination
 * @param[in ]           count                  number of bytes to read
 * @param[in ][out]     *fpos                   file position
 *
 * @return number of read bytes, -1 if error
 */
//==============================================================================
API_MOD_READ(CRCCU, void *device_handle, u8_t *dst, size_t count, u64_t *fpos)
{
        UNUSED_ARG(fpos);

        STOP_IF(device_handle == NULL);
        STOP_IF(dst == NULL);
        STOP_IF(count == 0);

        CRCCU *hdl = device_handle;

        ssize_t n = -1;

        if (device_is_access_granted(&hdl->file_lock)) {
                u32_t crc = CRC->DR;

                for (n = 0; n < (ssize_t)count && n < (ssize_t)sizeof(u32_t); n++) {
                        dst[n] = crc & 0xFF;
                        crc >>= 8;
                }
        } else {
                errno = EACCES;
        }

        return n;
}

//==============================================================================
/**
 * @brief IO control
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]           request                request
 * @param[in ][out]     *arg                    request's argument
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_IOCTL(CRCCU, void *device_handle, int request, void *arg)
{
        STOP_IF(device_handle == NULL);
        UNUSED_ARG(arg);

        CRCCU   *hdl    = device_handle;
        stdret_t status = STD_RET_ERROR;

        if (device_is_access_granted(&hdl->file_lock)) {
                switch (request) {
                default:
                        errno = EBADRQC;
                        break;
                }
        } else {
                errno = EACCES;
        }

        return status;
}

//==============================================================================
/**
 * @brief Flush device
 *
 * @param[in ]          *device_handle          device allocated memory
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_FLUSH(CRCCU, void *device_handle)
{
        STOP_IF(device_handle == NULL);

        return STD_RET_OK;
}

//==============================================================================
/**
 * @brief Device information
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[out]          *device_stat            device status
 *
 * @retval STD_RET_OK
 * @retval STD_RET_ERROR
 */
//==============================================================================
API_MOD_STAT(CRCCU, void *device_handle, struct vfs_dev_stat *device_stat)
{
        STOP_IF(device_handle == NULL);
        STOP_IF(device_stat == NULL);

        device_stat->st_major = _CRC_MAJOR_NUMBER;
        device_stat->st_minor = _CRC_MINOR_NUMBER;
        device_stat->st_size  = 4;

        return STD_RET_OK;
}

//==============================================================================
/**
 * @brief Reset CRC value register
 */
//==============================================================================
static inline void reset_CRC()
{
        CRC->CR = CRC_CR_RESET;
}

/*==============================================================================
  End of file
==============================================================================*/
