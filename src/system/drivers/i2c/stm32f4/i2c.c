/*=========================================================================*//**
@file    i2c.c

@author  Daniel Zorychta

@brief   This driver support I2C peripherals.

@note    Copyright (C) 2017  Daniel Zorychta <daniel.zorychta@gmail.com>

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

// NOTE: 10-bit addressing mode is experimental and not tested!

/*==============================================================================
  Include files
==============================================================================*/
#include "drivers/driver.h"
#include "stm32f4/i2c_cfg.h"
#include "stm32f4/stm32f4xx.h"
#include "../i2c_ioctl.h"
#include "lib/stm32f4xx_rcc.h"

#include "stm32f4/gpio_cfg.h" // TEST
#include "gpio_ddi.h" // TEST

/*==============================================================================
  Local symbolic constants/macros
==============================================================================*/
#define ACCESS_TIMEOUT         30000
#define DEVICE_TIMEOUT         2000
#define HEADER_ADDR_10BIT      0xF0

#define USE_DMA ((_I2C1_USE_DMA > 0) || (_I2C2_USE_DMA > 0) || (_I2C3_USE_DMA > 0))

/*==============================================================================
  Local types, enums definitions
==============================================================================*/
enum _I2C_major {
        #if defined(RCC_APB1ENR_I2C1EN)
        _I2C1,
        #endif
        #if defined(RCC_APB1ENR_I2C2EN)
        _I2C2,
        #endif
        #if defined(RCC_APB1ENR_I2C3EN)
        _I2C3,
        #endif
        _I2C_NUMBER_OF_PERIPHERALS
};

/// type defines configuration of single I2C peripheral
typedef struct {
    #if USE_DMA > 0
        const bool                use_DMA;              //!< peripheral uses DMA and IRQ (true), or only IRQ (false)
        const DMA_Stream_TypeDef *const DMA_tx;         //!< pointer to the DMA Tx channel peripheral
        const DMA_Stream_TypeDef *const DMA_rx;         //!< pointer to the DMA Rx channel peripheral
        const u8_t                DMA_tx_number;        //!< number of channel of DMA Tx
        const u8_t                DMA_rx_number;        //!< number of channel of DMA Rx
        const u8_t                DMA_channel;          //!< DMA channel number
        const IRQn_Type           DMA_tx_IRQ_n;         //!< number of interrupt in the vector table
        const IRQn_Type           DMA_rx_IRQ_n;         //!< number of interrupt in the vector table
    #endif
        const I2C_TypeDef        *const I2C;            //!< pointer to the I2C peripheral
        const u32_t               freq;                 //!< peripheral SCL frequency [Hz]
        const u32_t               APB1ENR_clk_mask;     //!< mask used to enable I2C clock in the APB1ENR register
        const u8_t                IRQ_prio;             //!< priority of IRQ (event, error, and DMA)
        const IRQn_Type           IRQ_EV_n;             //!< number of event IRQ vector
        const IRQn_Type           IRQ_ER_n;             //!< number of error IRQ vector
} I2C_info_t;

/// type defines I2C device in the runtime environment
typedef struct {
        I2C_config_t              config;               //!< pointer to the device configuration
        dev_lock_t                lock;                 //!< object used to lock access to opened device
        u8_t                      major;                //!< major number of the device (I2C peripheral number)
        u8_t                      minor;                //!< minor number of the device (device identifier)
} I2C_dev_t;

/// type defines main memory of this module
typedef struct {
        mutex_t                  *lock;                 //!< mutex used to lock access to the particular peripheral
        sem_t                    *event;                //!< semaphore used to indicate event (operation finished)
        u16_t                     SR1_mask;             //!< SR1 register mask (to catch specified event in IRQ)
        bool                      use_DMA:1;            //!< true if peripheral use DMA channels
        bool                      initialized:1;        //!< indicates that module for this peripheral is initialized
        u8_t                      dev_cnt;              //!< number of initialized devices
        u8_t                      error;                //!< error number (errno)
        u8_t                      unexp_event_cnt;      //!< number of unexpected events
} I2C_mem_t;


/*==============================================================================
  Local function prototypes
==============================================================================*/
static void release_resources(u8_t major);
static inline I2C_TypeDef *get_I2C(I2C_dev_t *hdl);
static int enable_I2C(u8_t major);
static void disable_I2C(u8_t major);
static bool _I2C_LLD__start(I2C_dev_t *hdl);
static bool _I2C_LLD__repeat_start(I2C_dev_t *hdl);
static void stop(I2C_dev_t *hdl);
static bool _I2C_LLD__send_address(I2C_dev_t *hdl, bool write);
static void clear_send_address_event(I2C_dev_t *hdl);
static bool send_subaddress(I2C_dev_t *hdl, u32_t address, I2C_sub_addr_mode_t mode);
static void set_ACK_according_to_reception_size(I2C_dev_t *hdl, size_t count);
static ssize_t receive(I2C_dev_t *hdl, u8_t *dst, size_t count);
static ssize_t transmit(I2C_dev_t *hdl, const u8_t *src, size_t count);
static void IRQ_EV_handler(u8_t major);
static void IRQ_ER_handler(u8_t major);

/*==============================================================================
  Local object definitions
==============================================================================*/
MODULE_NAME(I2C);

/// peripherals configuration
static const I2C_info_t I2C_INFO[_I2C_NUMBER_OF_PERIPHERALS] = {
        #if defined(RCC_APB1ENR_I2C1EN)
        {
                #if USE_DMA > 0
                .use_DMA          = _I2C1_USE_DMA,
                .DMA_tx           = DMA1_Stream6,
                .DMA_rx           = DMA1_Stream7,
                .DMA_tx_number    = 6,
                .DMA_rx_number    = 5,
                .DMA_channel      = 1,
                .DMA_tx_IRQ_n     = DMA1_Stream6_IRQn,
                .DMA_rx_IRQ_n     = DMA1_Stream5_IRQn,
                #endif
                .I2C              = I2C1,
                .freq             = _I2C1_FREQUENCY,
                .APB1ENR_clk_mask = RCC_APB1ENR_I2C1EN,
                .IRQ_prio         = _I2C1_IRQ_PRIO,
                .IRQ_EV_n         = I2C1_EV_IRQn,
                .IRQ_ER_n         = I2C1_ER_IRQn,
        },
        #endif
        #if defined(RCC_APB1ENR_I2C2EN)
        {
                #if USE_DMA > 0
                .use_DMA          = _I2C2_USE_DMA,
                .DMA_tx           = DMA1_Stream7,
                .DMA_rx           = DMA1_Stream3,
                .DMA_tx_number    = 7,
                .DMA_rx_number    = 3,
                .DMA_channel      = 7,
                .DMA_tx_IRQ_n     = DMA1_Stream7_IRQn,
                .DMA_rx_IRQ_n     = DMA1_Stream3_IRQn,
                #endif
                .I2C              = I2C2,
                .freq             = _I2C2_FREQUENCY,
                .APB1ENR_clk_mask = RCC_APB1ENR_I2C2EN,
                .IRQ_prio         = _I2C2_IRQ_PRIO,
                .IRQ_EV_n         = I2C2_EV_IRQn,
                .IRQ_ER_n         = I2C2_ER_IRQn,
        },
        #endif
        #if defined(RCC_APB1ENR_I2C3EN)
        {
                #if USE_DMA > 0
                .use_DMA          = _I2C3_USE_DMA,
                .DMA_tx           = DMA1_Stream4,
                .DMA_rx           = DMA1_Stream2,
                .DMA_tx_number    = 4,
                .DMA_rx_number    = 2,
                .DMA_channel      = 3,
                .DMA_tx_IRQ_n     = DMA1_Stream4_IRQn,
                .DMA_rx_IRQ_n     = DMA1_Stream2_IRQn,
                #endif
                .I2C              = I2C3,
                .freq             = _I2C3_FREQUENCY,
                .APB1ENR_clk_mask = RCC_APB1ENR_I2C3EN,
                .IRQ_prio         = _I2C3_IRQ_PRIO,
                .IRQ_EV_n         = I2C3_EV_IRQn,
                .IRQ_ER_n         = I2C3_ER_IRQn,
        },
        #endif
};

/// default peripheral configuration
static const I2C_config_t I2C_DEFAULT_CFG = {
        .address       = 0x00,
        .addr_10bit    = false,
        .sub_addr_mode = I2C_SUB_ADDR_MODE__DISABLED,
};

/// main memory of module
static I2C_mem_t *I2C[_I2C_NUMBER_OF_PERIPHERALS];

/*==============================================================================
  Exported object definitions
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
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_INIT(I2C, void **device_handle, u8_t major, u8_t minor)
{
        int err = ENODEV;

        if (major >= _I2C_NUMBER_OF_PERIPHERALS) {
                return err;
        }

        /* creates basic module structures */
        if (I2C[major] == NULL) {
                err = sys_zalloc(sizeof(I2C_mem_t), cast(void**, &I2C[major]));
                if (err != ESUCC) {
                        goto finish;
                }

                err = sys_mutex_create(MUTEX_TYPE_NORMAL, &I2C[major]->lock);
                if (err != ESUCC) {
                        goto finish;
                }

                err = sys_semaphore_create(1, 0, &I2C[major]->event);
                if (err != ESUCC) {
                        goto finish;
                }

                err = enable_I2C(major);
                if (err != ESUCC) {
                        goto finish;
                }
        }

        /* creates device structure */
        err = sys_zalloc(sizeof(I2C_dev_t), device_handle);
        if (err == ESUCC) {
                I2C_dev_t *hdl = *device_handle;
                hdl->config    = I2C_DEFAULT_CFG;
                hdl->major     = major;
                hdl->minor     = minor;

                sys_device_unlock(&hdl->lock, true);

                I2C[major]->dev_cnt++;
        }

        finish:
        if (err != ESUCC) {
                release_resources(major);
        }

        return err;
}

//==============================================================================
/**
 * @brief Release device
 *
 * @param[in ]          *device_handle          device allocated memory
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_RELEASE(I2C, void *device_handle)
{
        I2C_dev_t *hdl = device_handle;

        int err = sys_device_lock(&hdl->lock);
        if (!err) {
                I2C[hdl->major]->dev_cnt--;
                release_resources(hdl->major);
                sys_free(device_handle);
        }

        return err;
}

//==============================================================================
/**
 * @brief Open device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]           flags                  file operation flags (O_RDONLY, O_WRONLY, O_RDWR)
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_OPEN(I2C, void *device_handle, u32_t flags)
{
        UNUSED_ARG1(flags);

        I2C_dev_t *hdl = device_handle;

        return sys_device_lock(&hdl->lock);
}

//==============================================================================
/**
 * @brief Close device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]           force                  device force close (true)
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_CLOSE(I2C, void *device_handle, bool force)
{
        I2C_dev_t *hdl = device_handle;

        return sys_device_unlock(&hdl->lock, force);
}

//==============================================================================
/**
 * @brief Write data to device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]          *src                    data source
 * @param[in ]           count                  number of bytes to write
 * @param[in ][out]     *fpos                   file position
 * @param[out]          *wrcnt                  number of written bytes
 * @param[in ]           fattr                  file attributes
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_WRITE(I2C,
              void             *device_handle,
              const u8_t       *src,
              size_t            count,
              fpos_t           *fpos,
              size_t           *wrcnt,
              struct vfs_fattr  fattr)
{
        UNUSED_ARG1(fattr);

        I2C_dev_t *hdl = device_handle;

        int err = sys_mutex_lock(I2C[hdl->major]->lock, ACCESS_TIMEOUT);
        if (err == ESUCC) {

                if (!_I2C_LLD__start(hdl)) {
                        err = EIO;
                        goto error;
                }

                if (!_I2C_LLD__send_address(hdl, true)) {
                        err = ENXIO;
                        goto error;
                } else {
                        clear_send_address_event(hdl);
                }

                if (hdl->config.sub_addr_mode != I2C_SUB_ADDR_MODE__DISABLED) {
                        if (!send_subaddress(hdl, *fpos, hdl->config.sub_addr_mode)) {
                                err = EIO;
                                goto error;
                        }
                }

                *wrcnt = transmit(hdl, src, count);
                err    = I2C[hdl->major]->error;

                error:
                sys_mutex_unlock(I2C[hdl->major]->lock);
        }

        return err;
}

//==============================================================================
/**
 * @brief Read data from device
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[out]          *dst                    data destination
 * @param[in ]           count                  number of bytes to read
 * @param[in ][out]     *fpos                   file position
 * @param[out]          *rdcnt                  number of read bytes
 * @param[in ]           fattr                  file attributes
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_READ(I2C,
             void            *device_handle,
             u8_t            *dst,
             size_t           count,
             fpos_t          *fpos,
             size_t          *rdcnt,
             struct vfs_fattr fattr)
{
        UNUSED_ARG1(fattr);

        I2C_dev_t *hdl = device_handle;

        int err = sys_mutex_lock(I2C[hdl->major]->lock, ACCESS_TIMEOUT);
        if (err == ESUCC) {

                if (hdl->config.sub_addr_mode != I2C_SUB_ADDR_MODE__DISABLED) {
                        if (!_I2C_LLD__start(hdl)) {
                                err = EIO;
                                goto error;
                        }

                        if (!_I2C_LLD__send_address(hdl, true)) {
                                err = ENXIO;
                                goto error;
                        } else {
                                clear_send_address_event(hdl);
                        }

                        if (!send_subaddress(hdl, *fpos, hdl->config.sub_addr_mode)) {
                                err = EIO;
                                goto error;
                        }
                }

                if (!_I2C_LLD__repeat_start(hdl)) {
                        err = EIO;
                        goto error;
                }

                set_ACK_according_to_reception_size(hdl, count);
                if (!_I2C_LLD__send_address(hdl,  false)) {
                        err = ENXIO;
                        goto error;
                }

                *rdcnt = receive(hdl, dst, count);
                err    = I2C[hdl->major]->error;

                error:
                sys_mutex_unlock(I2C[hdl->major]->lock);
        }

        return err;
}

//==============================================================================
/**
 * @brief IO control
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[in ]           request                request
 * @param[in ][out]     *arg                    request's argument
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_IOCTL(I2C, void *device_handle, int request, void *arg)
{
        I2C_dev_t *hdl = device_handle;
        int        err = EINVAL;

        if (arg) {
                switch (request) {
                case IOCTL_I2C__CONFIGURE: {
                        const I2C_config_t *cfg   = arg;
                        hdl->config = *cfg;
                        err = ESUCC;
                        break;
                }

                default:
                        err = EBADRQC;
                        break;
                }
        }

        return err;
}

//==============================================================================
/**
 * @brief Flush device
 *
 * @param[in ]          *device_handle          device allocated memory
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_FLUSH(I2C, void *device_handle)
{
        UNUSED_ARG1(device_handle);

        return ESUCC;
}

//==============================================================================
/**
 * @brief Device information
 *
 * @param[in ]          *device_handle          device allocated memory
 * @param[out]          *device_stat            device status
 *
 * @return One of errno value (errno.h)
 */
//==============================================================================
API_MOD_STAT(I2C, void *device_handle, struct vfs_dev_stat *device_stat)
{
        I2C_dev_t *hdl = device_handle;

        device_stat->st_size  = 0;
        device_stat->st_major = hdl->major;
        device_stat->st_minor = hdl->minor;

        return ESUCC;
}

//==============================================================================
/**
 * @brief  Function release all resource allocated during initialization phase
 * @param  major         major device number
 * @return None
 */
//==============================================================================
static void release_resources(u8_t major)
{
        if (I2C[major] && I2C[major]->dev_cnt == 0) {
                if (I2C[major]->lock) {
                        sys_mutex_destroy(I2C[major]->lock);
                        I2C[major]->lock = NULL;
                }

                if (I2C[major]->event) {
                        sys_semaphore_destroy(I2C[major]->event);
                        I2C[major]->event = NULL;
                }

                if (I2C[major]->initialized) {
                        disable_I2C(major);
                }

                sys_free(cast(void**, &I2C[major]));
        }
}

//==============================================================================
/**
 * @brief  Returns I2C address of current device
 * @param  hdl          device handle
 * @return I2C peripheral address
 */
//==============================================================================
static inline I2C_TypeDef *get_I2C(I2C_dev_t *hdl)
{
        return const_cast(I2C_TypeDef*, I2C_INFO[hdl->major].I2C);
}

//==============================================================================
/**
 * @brief  Clear all DMA IRQ flags (of tx and rx)
 * @param  major        peripheral major number
 * @return None
 */
//==============================================================================
#if ((_I2C1_USE_DMA > 0) || (_I2C2_USE_DMA > 0))
static void clear_DMA_IRQ_flags(u8_t major)
{
        I2C_info_t         *cfg    = const_cast(I2C_info_t*, &I2C_INFO[major]);
        DMA_Stream_TypeDef *DMA_tx = const_cast(DMA_Stream_TypeDef*, cfg->DMA_tx);
        DMA_Stream_TypeDef *DMA_rx = const_cast(DMA_Stream_TypeDef*, cfg->DMA_rx);

        if (cfg->DMA_tx_number < 2) {
                DMA1->LIFCR = ( DMA_LIFCR_CFEIF0
                              | DMA_LIFCR_CDMEIF0
                              | DMA_LIFCR_CTEIF0
                              | DMA_LIFCR_CHTIF0
                              | DMA_LIFCR_CTCIF0 ) << (6 * (cfg->DMA_tx_number - 0));
        } else if (cfg->DMA_tx_number < 4) {
                DMA1->LIFCR = ( DMA_LIFCR_CFEIF2
                              | DMA_LIFCR_CDMEIF2
                              | DMA_LIFCR_CTEIF2
                              | DMA_LIFCR_CHTIF2
                              | DMA_LIFCR_CTCIF2 ) << (6 * (cfg->DMA_tx_number - 2));
        } else if (cfg->DMA_tx_number < 6) {
                DMA1->HIFCR = ( DMA_HIFCR_CFEIF4
                              | DMA_HIFCR_CDMEIF4
                              | DMA_HIFCR_CTEIF4
                              | DMA_HIFCR_CHTIF4
                              | DMA_HIFCR_CTCIF4 ) << (6 * (cfg->DMA_tx_number - 4));
        } else if (cfg->DMA_tx_number < 8) {
                DMA1->HIFCR = ( DMA_HIFCR_CFEIF6
                              | DMA_HIFCR_CDMEIF6
                              | DMA_HIFCR_CTEIF6
                              | DMA_HIFCR_CHTIF6
                              | DMA_HIFCR_CTCIF6 ) << (6 * (cfg->DMA_tx_number - 6));
        }

        CLEAR_BIT(DMA_tx->CR, DMA_SxCR_EN);
        CLEAR_BIT(DMA_rx->CR, DMA_SxCR_EN);
}
#endif

//==============================================================================
/**
 * @brief  Enables selected I2C peripheral according with configuration
 * @param  major        peripheral number
 * @return One of errno value.
 */
//==============================================================================
static int enable_I2C(u8_t major)
{
        const I2C_info_t *cfg = &I2C_INFO[major];
        I2C_TypeDef            *i2c = const_cast(I2C_TypeDef*, I2C_INFO[major].I2C);

        SET_BIT(RCC->APB1RSTR, cfg->APB1ENR_clk_mask);
        CLEAR_BIT(RCC->APB1RSTR, cfg->APB1ENR_clk_mask);

        CLEAR_BIT(RCC->APB1ENR, cfg->APB1ENR_clk_mask);
        SET_BIT(RCC->APB1ENR, cfg->APB1ENR_clk_mask);

        RCC_ClocksTypeDef clocks;
        memset(&clocks, 0, sizeof(RCC_ClocksTypeDef));
        RCC_GetClocksFreq(&clocks);
        clocks.PCLK1_Frequency /= 1000000;

        if (clocks.PCLK1_Frequency < 2) {
                return EIO;
        }

        I2C[major]->use_DMA = false;

#if USE_DMA > 0
        if (cfg->use_DMA) {
                if (!(cfg->DMA_rx->CR & DMA_SxCR_EN) && !(cfg->DMA_tx->CR & DMA_SxCR_EN)) {
                        SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_DMA1EN);

                        clear_DMA_IRQ_flags(major);

                        NVIC_EnableIRQ(cfg->DMA_tx_IRQ_n);
                        NVIC_EnableIRQ(cfg->DMA_rx_IRQ_n);
                        NVIC_SetPriority(cfg->DMA_tx_IRQ_n, cfg->IRQ_prio);
                        NVIC_SetPriority(cfg->DMA_rx_IRQ_n, cfg->IRQ_prio);

                        I2C[major]->use_DMA = true;
                }
        }
#endif

        NVIC_EnableIRQ(cfg->IRQ_EV_n);
        NVIC_EnableIRQ(cfg->IRQ_ER_n);
        NVIC_SetPriority(cfg->IRQ_EV_n, cfg->IRQ_prio);
        NVIC_SetPriority(cfg->IRQ_ER_n, cfg->IRQ_prio);

        u16_t CCR;
        if (cfg->freq <= 100000) {
                CCR = ((1000000/(2 * cfg->freq)) * clocks.PCLK1_Frequency) & I2C_CCR_CCR;
        } else if (cfg->freq < 400000){
                CCR  = ((10000000/(3 * cfg->freq)) * clocks.PCLK1_Frequency / 10) & I2C_CCR_CCR;
                CCR |= I2C_CCR_FS;
        } else {
                CCR  = ((100000000/(25 * cfg->freq)) * clocks.PCLK1_Frequency / 100 + 1) & I2C_CCR_CCR;
                CCR |= I2C_CCR_FS | I2C_CCR_DUTY;
        }

        i2c->CR1   = I2C_CR1_SWRST;
        i2c->CR1   = 0;
        i2c->CR2   = clocks.PCLK1_Frequency & I2C_CR2_FREQ;;
        i2c->CCR   = CCR;
        i2c->TRISE = clocks.PCLK1_Frequency + 1;
        i2c->CR1   = I2C_CR1_PE;

        I2C[major]->initialized = true;

        return ESUCC;
}

//==============================================================================
/**
 * @brief  Disables selected I2C peripheral
 * @param  major        I2C peripheral number
 * @return None
 */
//==============================================================================
static void disable_I2C(u8_t major)
{
        const I2C_info_t *cfg = &I2C_INFO[major];
        I2C_TypeDef            *i2c = const_cast(I2C_TypeDef*, I2C_INFO[major].I2C);

#if USE_DMA > 0
        if (I2C[major]->use_DMA) {
                NVIC_DisableIRQ(cfg->DMA_tx_IRQ_n);
                NVIC_DisableIRQ(cfg->DMA_rx_IRQ_n);

                DMA_Stream_TypeDef *DMA_tx = const_cast(DMA_Stream_TypeDef*, cfg->DMA_tx);
                DMA_Stream_TypeDef *DMA_rx = const_cast(DMA_Stream_TypeDef*, cfg->DMA_rx);

                DMA_rx->CR = 0;
                DMA_tx->CR = 0;
        }
#endif

        NVIC_DisableIRQ(cfg->IRQ_EV_n);
        NVIC_DisableIRQ(cfg->IRQ_ER_n);

        WRITE_REG(i2c->CR1, 0);
        SET_BIT(RCC->APB1RSTR, cfg->APB1ENR_clk_mask);
        CLEAR_BIT(RCC->APB1RSTR, cfg->APB1ENR_clk_mask);
        CLEAR_BIT(RCC->APB1ENR, cfg->APB1ENR_clk_mask);

        I2C[major]->initialized = false;
}

//==============================================================================
/**
 * @brief  Function handle error (try make the interface working)
 * @param  hdl          device handle
 * @return None
 */
//==============================================================================
static void error(I2C_dev_t *hdl)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

//        errno = I2C->periph[hdl->config->major].error;

        CLEAR_BIT(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);

        u8_t tmp = i2c->SR1;
             tmp = i2c->SR2;
             tmp = i2c->DR;
             tmp = i2c->DR;
        (void) tmp;

        i2c->SR1 = 0;

        stop(hdl);
        sys_sleep_ms(1);

        if (I2C[hdl->major]->error == EIO) {
                enable_I2C(hdl->major);
        }
}

//==============================================================================
/**
 * @brief  Function wait for selected event (IRQ)
 * @param  hdl                  device handle
 * @param  SR1_event_mask       event mask (bits from SR1 register)
 * @return On success true is returned, otherwise false
 */
//==============================================================================
static bool wait_for_I2C_event(I2C_dev_t *hdl, u16_t SR1_event_mask)
{
        I2C[hdl->major]->SR1_mask = SR1_event_mask & 0xDF;
        I2C[hdl->major]->error    = 0;

        u16_t CR2 = I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
        if (SR1_event_mask & (I2C_SR1_RXNE | I2C_SR1_TXE)) {
                CR2 |= I2C_CR2_ITBUFEN;
        }

        SET_BIT(get_I2C(hdl)->CR2, CR2);

        if (sys_semaphore_wait(I2C[hdl->major]->event, DEVICE_TIMEOUT) == ESUCC) {
                if (I2C[hdl->major]->error == 0) {
                        return true;
                }
        } else {
                I2C[hdl->major]->error = EIO;
        }

        error(hdl);
        return false;
}

//==============================================================================
/**
 * @brief  Function wait for DMA event (IRQ)
 * @param  hdl                  device handle
 * @param  DMA                  DMA channel
 * @return On success true is returned, otherwise false
 */
//==============================================================================
#if USE_DMA > 0
static bool wait_for_DMA_event(I2C_dev_t *hdl, DMA_Stream_TypeDef *DMA)
{
        I2C[hdl->major]->error = 0;

        I2C_TypeDef *i2c = get_I2C(hdl);
        CLEAR_BIT(i2c->CR2, I2C_CR2_ITBUFEN | I2C_CR2_ITERREN | I2C_CR2_ITEVTEN);
        SET_BIT(i2c->CR2, I2C_CR2_DMAEN);
        SET_BIT(i2c->CR2, I2C_CR2_LAST);
        SET_BIT(DMA->CR, DMA_SxCR_EN);

        if (sys_semaphore_wait(I2C[hdl->major]->event, DEVICE_TIMEOUT) == ESUCC) {
                if (I2C[hdl->major]->error == 0) {
                        return true;
                }
        } else {
                I2C[hdl->major]->error = EIO;
        }

        error(hdl);
        return false;
}
#endif

//==============================================================================
/**
 * @brief  Function generate START sequence on I2C bus
 * @param  hdl                  device handle
 * @return On success true is returned, otherwise false
 */
//==============================================================================
static bool _I2C_LLD__start(I2C_dev_t *hdl)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        _GPIO_DDI_set_pin(IOCTL_GPIO_PORT_IDX__TEST1, IOCTL_GPIO_PIN_IDX__TEST1);

        SET_BIT(i2c->CR1, I2C_CR1_START);

        _GPIO_DDI_clear_pin(IOCTL_GPIO_PORT_IDX__TEST1, IOCTL_GPIO_PIN_IDX__TEST1);

        bool r =  wait_for_I2C_event(hdl, I2C_SR1_SB);


        return r;
}

static bool _I2C_LLD__repeat_start(I2C_dev_t *hdl)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        _GPIO_DDI_set_pin(IOCTL_GPIO_PORT_IDX__TEST1, IOCTL_GPIO_PIN_IDX__TEST1);

        SET_BIT(i2c->CR1, I2C_CR1_START);

        while (!(i2c->SR1 & I2C_SR1_SB)); // TEST i to działa dobrze

        _GPIO_DDI_clear_pin(IOCTL_GPIO_PORT_IDX__TEST1, IOCTL_GPIO_PIN_IDX__TEST1);

        return true;
//        bool r =  wait_for_I2C_event(hdl, I2C_SR1_SB);


//        return r;
}

//==============================================================================
/**
 * @brief  Function generate STOP sequence on I2C bus
 * @param  hdl                  device handle
 * @return On success true is returned, otherwise false
 */
//==============================================================================
static void stop(I2C_dev_t *hdl)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        SET_BIT(i2c->CR1, I2C_CR1_STOP);
}

//==============================================================================
/**
 * @brief  Function send I2C address sequence
 * @param  hdl                  device handle
 * @param  write                true: compose write address
 * @return On success true is returned, otherwise false
 */
//==============================================================================
static bool _I2C_LLD__send_address(I2C_dev_t *hdl, bool write)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        if (hdl->config.addr_10bit) {
                u8_t hdr = HEADER_ADDR_10BIT | ((hdl->config.address >> 7) & 0x6);

                // send header + 2 most significant bits of 10-bit address
                i2c->DR = HEADER_ADDR_10BIT | ((hdl->config.address & 0xFE) >> 7);
                if (!wait_for_I2C_event(hdl, I2C_SR1_ADD10)) {
                        return false;
                }

                // send rest 8 bits of 10-bit address
                u8_t  addr = hdl->config.address & 0xFF;
                u16_t tmp  = i2c->SR1;
                (void)tmp;   i2c->DR = addr;
                if (!wait_for_I2C_event(hdl, I2C_SR1_ADDR)) {
                        return false;
                }

                clear_send_address_event(hdl);

                // send repeat start
                if (!_I2C_LLD__start(hdl)) {
                        return false;
                }

                // send header
                i2c->DR = write ? hdr : hdr | 0x01;
                return wait_for_I2C_event(hdl, I2C_SR1_ADDR);

        } else {
                u16_t tmp = i2c->SR1;
                (void)tmp;  i2c->DR = write ? hdl->config.address & 0xFE : hdl->config.address | 0x01;
                return wait_for_I2C_event(hdl, I2C_SR1_ADDR);
        }
}

//==============================================================================
/**
 * @brief  Clear event of send address
 * @param  hdl                  device handle
 * @return None
 */
//==============================================================================
static void clear_send_address_event(I2C_dev_t *hdl)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        sys_critical_section_begin();
        {
                u16_t tmp;
                tmp = i2c->SR1;
                tmp = i2c->SR2;
                (void)tmp;
        }
        sys_critical_section_end();
}

//==============================================================================
/**
 * @brief  Function send subaddress to I2C device
 * @param  hdl                  device handle
 * @param  address              subaddress
 * @param  mode                 size of subaddress
 * @return On success true is returned, otherwise false
 */
//==============================================================================
static bool send_subaddress(I2C_dev_t *hdl, u32_t address, I2C_sub_addr_mode_t mode)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        switch (mode) {
        case I2C_SUB_ADDR_MODE__3_BYTES:
                i2c->DR = address >> 16;
                if (!wait_for_I2C_event(hdl, I2C_SR1_BTF)) {
                        break;
                } else {
                        // if there is no error then send next bytes
                }

        case I2C_SUB_ADDR_MODE__2_BYTES:
                i2c->DR = address >> 8;
                if (!wait_for_I2C_event(hdl, I2C_SR1_BTF)) {
                        break;
                } else {
                        // if there is no error then send next bytes
                }

        case I2C_SUB_ADDR_MODE__1_BYTE:
                i2c->DR = address & 0xFF;
                return wait_for_I2C_event(hdl, I2C_SR1_BTF);

        default:
                return false;
        }

        return true;
}

//==============================================================================
/**
 * @brief  Function set ACK/NACK status according to transfer size
 * @param  hdl                  device handle
 * @param  count                transfer size (bytes)
 * @return None
 */
//==============================================================================
static void set_ACK_according_to_reception_size(I2C_dev_t *hdl, size_t count)
{
        I2C_TypeDef *i2c = get_I2C(hdl);

        if (count == 2) {
                SET_BIT(i2c->CR1, I2C_CR1_POS | I2C_CR1_ACK);
        } else {
                SET_BIT(i2c->CR1, I2C_CR1_ACK);
        }
}

//==============================================================================
/**
 * @brief  Function receive bytes from I2C bus (master-receiver)
 * @param  hdl                  device handle
 * @param  dst                  destination buffer
 * @param  count                number of bytes to receive
 * @return Number of received bytes
 */
//==============================================================================
static ssize_t receive(I2C_dev_t *hdl, u8_t *dst, size_t count)
{
        ssize_t      n   = 0;
        I2C_TypeDef *i2c = get_I2C(hdl);

        if (count >= 3) {
                clear_send_address_event(hdl);

#if USE_DMA > 0
                if (I2C[hdl->major]->use_DMA) {
                        DMA_Stream_TypeDef *DMA = const_cast(DMA_Stream_TypeDef*,
                                                             I2C_INFO[hdl->major].DMA_rx);

                        DMA->PAR  = cast(u32_t, &i2c->DR);
                        DMA->M0AR = cast(u32_t, dst);
                        DMA->NDTR = count;

                        DMA->CR = ((I2C_INFO[hdl->major].DMA_channel & 7) << DMA_SxCR_CHSEL_Pos)
                                | DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE;

                        if (wait_for_DMA_event(hdl, DMA)) {
                                n = count;
                        }
                } else {
#else
                {
#endif
                        while (count) {
                                if (count == 3) {
                                        if (!wait_for_I2C_event(hdl, I2C_SR1_BTF)) {
                                                break;
                                        }

                                        CLEAR_BIT(i2c->CR1, I2C_CR1_ACK);
                                        *dst++ = i2c->DR;
                                        n++;

                                        if (!wait_for_I2C_event(hdl, I2C_SR1_RXNE)) {
                                                break;
                                        }

                                        stop(hdl);
                                        *dst++ = i2c->DR;
                                        n++;

                                        if (!wait_for_I2C_event(hdl, I2C_SR1_RXNE)) {
                                                break;
                                        }

                                        *dst++ = i2c->DR;
                                        n++;

                                        count = 0;
                                } else {
                                        if (wait_for_I2C_event(hdl, I2C_SR1_RXNE)) {
                                                *dst++ = i2c->DR;
                                                count--;
                                                n++;
                                        } else {
                                                break;
                                        }
                                }
                        }
                }

        } else if (count == 2) {
                sys_critical_section_begin();
                {
                        clear_send_address_event(hdl);
                        CLEAR_BIT(i2c->CR1, I2C_CR1_ACK);
                }
                sys_critical_section_end();

                if (wait_for_I2C_event(hdl, I2C_SR1_BTF)) {
                        stop(hdl);

                        *dst++ = i2c->DR;
                        *dst++ = i2c->DR;
                        n     += 2;
                }

        } else if (count == 1) {
                CLEAR_BIT(i2c->CR1, I2C_CR1_ACK);
                clear_send_address_event(hdl);
                stop(hdl);

                if (wait_for_I2C_event(hdl, I2C_SR1_RXNE)) {
                        *dst++ = i2c->DR;
                        n     += 1;
                }
        }

        stop(hdl);

        return n;
}

//==============================================================================
/**
 * @brief  Function transmit selected amount bytes to I2C bus
 * @param  hdl                  device handle
 * @param  src                  data source
 * @param  count                number of bytes to transfer
 * @return Number of written bytes
 */
//==============================================================================
static ssize_t transmit(I2C_dev_t *hdl, const u8_t *src, size_t count)
{
        ssize_t      n    = 0;
        I2C_TypeDef *i2c  = get_I2C(hdl);
        bool         succ = false;

        clear_send_address_event(hdl);

#if USE_DMA > 0
        if (count >= 3 && I2C[hdl->major]->use_DMA) {
                DMA_Stream_TypeDef *DMA = const_cast(DMA_Stream_TypeDef*,
                                                     I2C_INFO[hdl->major].DMA_tx);

                DMA->PAR  = cast(u32_t, &i2c->DR);
                DMA->M0AR = cast(u32_t, src);
                DMA->NDTR = count;

                DMA->CR = ((I2C_INFO[hdl->major].DMA_channel & 7) << DMA_SxCR_CHSEL_Pos)
                        | DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DIR_0;

                if (wait_for_DMA_event(hdl, DMA)) {
                        n    = count;
                        succ = true;
                }
        } else {
#else
        {
#endif
                succ = true;
                while (count) {
                        if (wait_for_I2C_event(hdl, I2C_SR1_TXE)) {
                                i2c->DR = *src++;
                        } else {
                                succ = false;
                                break;
                        }

                        n++;
                        count--;
                }
        }

        if (n && succ) {
                if (!wait_for_I2C_event(hdl, I2C_SR1_BTF))
                        return n - 1;
        }

        stop(hdl);

        return n;
}

//==============================================================================
/**
 * @brief  Event IRQ handler (transaction state machine)
 * @param  major        number of peripheral
 * @return If IRQ was woken then true is returned, otherwise false
 */
//==============================================================================
static void IRQ_EV_handler(u8_t major)
{
//        _GPIO_DDI_set_pin(IOCTL_GPIO_PORT_IDX__TEST2, IOCTL_GPIO_PIN_IDX__TEST2);

        bool woken = false;

        I2C_TypeDef *i2c = const_cast(I2C_TypeDef*, I2C_INFO[major].I2C);
        u16_t SR1 = i2c->SR1;
        u16_t SR2 = i2c->SR2;
        UNUSED_ARG1(SR2);

        if (SR1 & I2C[major]->SR1_mask) {
                sys_semaphore_signal_from_ISR(I2C[major]->event, &woken);
                CLEAR_BIT(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
                I2C[major]->unexp_event_cnt = 0;
        } else {
                /*
                 * This counter is used to check if there is no death loop of
                 * not handled IRQ. If counter reach specified value then
                 * the error flag is set.
                 */
                if (++I2C[major]->unexp_event_cnt >= 16) {
                        I2C[major]->error = EIO;
                        sys_semaphore_signal_from_ISR(I2C[major]->event, &woken);
                        CLEAR_BIT(I2C1->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
                }
        }

        sys_thread_yield_from_ISR(woken);

//        _GPIO_DDI_clear_pin(IOCTL_GPIO_PORT_IDX__TEST2, IOCTL_GPIO_PIN_IDX__TEST2);
}

//==============================================================================
/**
 * @brief  Error IRQ handler
 * @param  major        number of peripheral
 * @return If IRQ was woken then true is returned, otherwise false
 */
//==============================================================================
static void IRQ_ER_handler(u8_t major)
{
        I2C_TypeDef *i2c = const_cast(I2C_TypeDef*, I2C_INFO[major].I2C);

        if (i2c->SR1 & I2C_SR1_ARLO) {
                I2C[major]->error = EAGAIN;

        } else if (i2c->SR1 & I2C_SR1_AF) {
                if (I2C[major]->SR1_mask & (I2C_SR1_ADDR | I2C_SR1_ADD10))
                        I2C[major]->error = ENXIO;
                else
                        I2C[major]->error = EIO;
        } else {
                I2C[major]->error = EIO;
        }

        // clear error flags
        i2c->SR1 = 0;

        bool woken = false;
        sys_semaphore_signal_from_ISR(I2C[major]->event, &woken);
        CLEAR_BIT(i2c->CR2, I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
        sys_thread_yield_from_ISR(woken);
}

//==============================================================================
/**
 * @brief  DMA IRQ handler
 * @param  DMA_ch_no    DMA channel number
 * @param  major        number of peripheral
 * @return If IRQ was woken then true is returned, otherwise false
 */
//==============================================================================
#if USE_DMA > 0
static void IRQ_DMA_handler(const int DMA_ch_no, u8_t major)
{
        bool is_error = false;

        if (DMA_ch_no < 2) {
                is_error = (DMA1->LISR & DMA_LISR_TEIF0) << (6 * (DMA_ch_no - 0));
        } else if (DMA_ch_no < 4) {
                is_error = (DMA1->LISR & DMA_LISR_TEIF2) << (6 * (DMA_ch_no - 2));
        } else if (DMA_ch_no < 6) {
                is_error = (DMA1->HISR & DMA_HISR_TEIF4) << (6 * (DMA_ch_no - 4));
        } else if (DMA_ch_no < 8) {
                is_error = (DMA1->HISR & DMA_HISR_TEIF6) << (6 * (DMA_ch_no - 6));
        }

        if (is_error) {
                I2C[major]->error = EIO;
        }

        bool woken = false;
        sys_semaphore_signal_from_ISR(I2C[major]->event, &woken);
        clear_DMA_IRQ_flags(major);
        sys_thread_yield_from_ISR(woken);
}
#endif

//==============================================================================
/**
 * @brief  I2C1 Event IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
#if defined(RCC_APB1ENR_I2C1EN)
void I2C1_EV_IRQHandler(void)
{
        IRQ_EV_handler(_I2C1);
}
#endif

//==============================================================================
/**
 * @brief  I2C1 Error IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
#if defined(RCC_APB1ENR_I2C1EN)
void I2C1_ER_IRQHandler(void)
{
        IRQ_ER_handler(_I2C1);
}
#endif

//==============================================================================
/**
 * @brief  I2C2 Event IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
#if defined(RCC_APB1ENR_I2C2EN)
void I2C2_EV_IRQHandler(void)
{
        IRQ_EV_handler(_I2C2);
}
#endif

//==============================================================================
/**
 * @brief  I2C2 Error IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
#if defined(RCC_APB1ENR_I2C2EN)
void I2C2_ER_IRQHandler(void)
{
        IRQ_ER_handler(_I2C2);
}
#endif

//==============================================================================
/**
 * @brief  I2C3 Event IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
#if defined(RCC_APB1ENR_I2C3EN)
void I2C3_EV_IRQHandler(void)
{
        IRQ_EV_handler(_I2C3);
}
#endif

//==============================================================================
/**
 * @brief  I2C3 Error IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
#if defined(RCC_APB1ENR_I2C3EN)
void I2C3_ER_IRQHandler(void)
{
        IRQ_ER_handler(_I2C3);
}
#endif

#if defined(RCC_APB1ENR_I2C1EN) && (_I2C1_USE_DMA > 0)
//==============================================================================
/**
 * @brief  I2C1 DMA Tx Complete IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
void DMA1_Stream6_IRQHandler(void)
{
        IRQ_DMA_handler(I2C_INFO[_I2C1].DMA_tx_number, _I2C1);
}

//==============================================================================
/**
 * @brief  I2C1 DMA Rx Complete IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
void DMA1_Stream5_IRQHandler(void)
{
        IRQ_DMA_handler(I2C_INFO[_I2C1].DMA_rx_number, _I2C1);
}
#endif

#if defined(RCC_APB1ENR_I2C2EN) && (_I2C2_USE_DMA > 0)
//==============================================================================
/**
 * @brief  I2C2 DMA Tx Complete IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
void DMA1_Stream7_IRQHandler(void)
{
        IRQ_DMA_handler(I2C_INFO[_I2C2].DMA_tx_number, _I2C2);
}

//==============================================================================
/**
 * @brief  I2C2 DMA Rx Complete IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
void DMA1_Stream3_IRQHandler(void)
{
        IRQ_DMA_handler(I2C_INFO[_I2C2].DMA_rx_number, _I2C2);
}
#endif

#if defined(RCC_APB1ENR_I2C3EN) && (_I2C3_USE_DMA > 0)
//==============================================================================
/**
 * @brief  I2C2 DMA Tx Complete IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
void DMA1_Stream4_IRQHandler(void)
{
        IRQ_DMA_handler(I2C_INFO[_I2C3].DMA_tx_number, _I2C3);
}

//==============================================================================
/**
 * @brief  I2C2 DMA Rx Complete IRQ handler
 * @param  None
 * @return None
 */
//==============================================================================
void DMA1_Stream2_IRQHandler(void)
{
        IRQ_DMA_handler(I2C_INFO[_I2C3].DMA_rx_number, _I2C3);
}
#endif

/*==============================================================================
  End of file
==============================================================================*/
