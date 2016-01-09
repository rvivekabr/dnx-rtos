/*=========================================================================*//**
@file    uart.c

@author  Daniel Zorychta

@brief   This file support USART peripherals

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

/*==============================================================================
  Include files
==============================================================================*/
#include "drivers/driver.h"
#include "stm32f1/uart_cfg.h"
#include "stm32f1/stm32f10x.h"
#include "stm32f1/lib/stm32f10x_rcc.h"
#include "../uart_ioctl.h"

/*==============================================================================
  Local symbolic constants/macros
==============================================================================*/
#define RELEASE_TIMEOUT                         100
#define TX_WAIT_TIMEOUT                         MAX_DELAY_MS
#define RX_WAIT_TIMEOUT                         MAX_DELAY_MS
#define MTX_BLOCK_TIMEOUT                       MAX_DELAY_MS

/*==============================================================================
  Local types, enums definitions
==============================================================================*/
//* UARTs */
enum {
        #if defined(RCC_APB2ENR_USART1EN)
        _UART1,
        #endif
        #if defined(RCC_APB1ENR_USART2EN)
        _UART2,
        #endif
        #if defined(RCC_APB1ENR_USART3EN)
        _UART3,
        #endif
        #if defined(RCC_APB1ENR_UART4EN)
        _UART4,
        #endif
        #if defined(RCC_APB1ENR_UART5EN)
        _UART5,
        #endif
        _UART_COUNT
};

/* UART registers */
typedef struct {
        USART_t        *UART;
        __IO uint32_t  *APBENR;
        __IO uint32_t  *APBRSTR;
        const uint32_t  APBENR_UARTEN;
        const uint32_t  APBRSTR_UARTRST;
        const IRQn_Type IRQn;
        const u32_t     PRIORITY;
} UART_regs_t;

/* USART handling structure */
struct UART_mem {
        // Rx FIFO
        struct Rx_FIFO {
                u8_t            buffer[_UART_RX_BUFFER_SIZE];
                u16_t           buffer_level;
                u16_t           read_index;
                u16_t           write_index;
        } Rx_FIFO;

        // Tx FIFO
        struct Tx_buffer {
                const u8_t     *src_ptr;
                size_t          data_size;
        } Tx_buffer;

        // UART control
        sem_t                  *data_write_sem;
        sem_t                  *data_read_sem;
        mutex_t                *port_lock_rx_mtx;
        mutex_t                *port_lock_tx_mtx;
        u8_t                    major;
        struct UART_config      config;
};

/*==============================================================================
  Local function prototypes
==============================================================================*/
static int  UART_turn_on  (const UART_regs_t *UART);
static int  UART_turn_off (const UART_regs_t *UART);
static void UART_configure(u8_t major, const struct UART_config *config);
static bool FIFO_write    (struct Rx_FIFO *fifo, u8_t *data);
static bool FIFO_read     (struct Rx_FIFO *fifo, u8_t *data);
static void IRQ_handle    (u8_t major);

/*==============================================================================
  Local object definitions
==============================================================================*/
MODULE_NAME(UART);

// all registers which are need to control particular UART peripheral
static const UART_regs_t UART[] = {
        #if defined(RCC_APB2ENR_USART1EN)
        {
                .UART            = USART1,
                .APBENR          = &RCC->APB2ENR,
                .APBRSTR         = &RCC->APB2RSTR,
                .APBENR_UARTEN   = RCC_APB2ENR_USART1EN,
                .APBRSTR_UARTRST = RCC_APB2RSTR_USART1RST,
                .IRQn            = USART1_IRQn,
                .PRIORITY        = _UART1_IRQ_PRIORITY
        },
        #endif
        #if defined(RCC_APB1ENR_USART2EN)
        {
                .UART            = USART2,
                .APBENR          = &RCC->APB1ENR,
                .APBRSTR         = &RCC->APB1RSTR,
                .APBENR_UARTEN   = RCC_APB1ENR_USART2EN,
                .APBRSTR_UARTRST = RCC_APB1RSTR_USART2RST,
                .IRQn            = USART2_IRQn,
                .PRIORITY        = _UART2_IRQ_PRIORITY
        },
        #endif
        #if defined(RCC_APB1ENR_USART3EN)
        {
                .UART            = USART3,
                .APBENR          = &RCC->APB1ENR,
                .APBRSTR         = &RCC->APB1RSTR,
                .APBENR_UARTEN   = RCC_APB1ENR_USART3EN,
                .APBRSTR_UARTRST = RCC_APB1RSTR_USART3RST,
                .IRQn            = USART3_IRQn,
                .PRIORITY        = _UART3_IRQ_PRIORITY
        },
        #endif
        #if defined(RCC_APB1ENR_UART4EN)
        {
                .UART            = UART4,
                .APBENR          = &RCC->APB1ENR,
                .APBRSTR         = &RCC->APB1RSTR,
                .APBENR_UARTEN   = RCC_APB1ENR_UART4EN,
                .APBRSTR_UARTRST = RCC_APB1RSTR_UART4RST,
                .IRQn            = UART4_IRQn,
                .PRIORITY        = _UART4_IRQ_PRIORITY
        },
        #endif
        #if defined(RCC_APB1ENR_UART5EN)
        {
                .UART            = UART5,
                .APBENR          = &RCC->APB1ENR,
                .APBRSTR         = &RCC->APB1RSTR,
                .APBENR_UARTEN   = RCC_APB1ENR_UART5EN,
                .APBRSTR_UARTRST = RCC_APB1RSTR_UART5RST,
                .IRQn            = UART5_IRQn,
                .PRIORITY        = _UART5_IRQ_PRIORITY
        }
        #endif
};

/* UART default configuration */
static const struct UART_config UART_DEFAULT_CONFIG = {
        .parity             = _UART_DEFAULT_PARITY,
        .stop_bits          = _UART_DEFAULT_STOP_BITS,
        .LIN_break_length   = _UART_DEFAULT_LIN_BREAK_LEN,
        .tx_enable          = _UART_DEFAULT_TX_ENABLE,
        .rx_enable          = _UART_DEFAULT_RX_ENABLE,
        .LIN_mode_enable    = _UART_DEFAULT_LIN_MODE_ENABLE,
        .hardware_flow_ctrl = _UART_DEFAULT_HW_FLOW_CTRL,
        .single_wire_mode   = _UART_DEFAULT_SINGLE_WIRE_MODE,
        .baud               = _UART_DEFAULT_BAUD
};

/* structure which identify USARTs data in the IRQs */
static struct UART_mem *UART_mem[_UART_COUNT];

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
API_MOD_INIT(UART, void **device_handle, u8_t major, u8_t minor)
{
        UNUSED_ARG1(minor);

        if (major >= _UART_COUNT || minor != 0) {
                return ENODEV;
        }

        int result = sys_zalloc(sizeof(struct UART_mem), device_handle);
        if (result == ESUCC) {
                UART_mem[major] = *device_handle;

                result = sys_semaphore_create(1, 0, &UART_mem[major]->data_write_sem);
                if (result != ESUCC)
                        goto finish;

                result = sys_semaphore_create(_UART_RX_BUFFER_SIZE, 0, &UART_mem[major]->data_read_sem);
                if (result != ESUCC)
                        goto finish;

                result = sys_mutex_create(MUTEX_TYPE_NORMAL, &UART_mem[major]->port_lock_rx_mtx);
                if (result != ESUCC)
                        goto finish;

                result = sys_mutex_create(MUTEX_TYPE_NORMAL, &UART_mem[major]->port_lock_tx_mtx);
                if (result != ESUCC)
                        goto finish;

                result = UART_turn_on(&UART[major]);
                if (result == ESUCC) {
                        UART_mem[major]->major  = major;
                        UART_mem[major]->config = UART_DEFAULT_CONFIG;
                        NVIC_EnableIRQ(UART[major].IRQn);
                        NVIC_SetPriority(UART[major].IRQn, UART[major].PRIORITY);
                        UART_configure(major, &UART_DEFAULT_CONFIG);
                }

                finish:
                if (result != ESUCC) {
                        if (UART_mem[major]->port_lock_tx_mtx)
                                sys_mutex_destroy(UART_mem[major]->port_lock_tx_mtx);

                        if (UART_mem[major]->port_lock_rx_mtx)
                                sys_mutex_destroy(UART_mem[major]->port_lock_rx_mtx);

                        if (UART_mem[major]->data_write_sem)
                                sys_semaphore_destroy(UART_mem[major]->data_write_sem);

                        if (UART_mem[major]->data_write_sem)
                                sys_semaphore_destroy(UART_mem[major]->data_write_sem);

                        sys_free(device_handle);
                        UART_mem[major] = NULL;
                }
        }

        return result;
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
API_MOD_RELEASE(UART, void *device_handle)
{
        struct UART_mem *hdl = device_handle;

        if (sys_mutex_lock(hdl->port_lock_rx_mtx, RELEASE_TIMEOUT) == ESUCC) {
                if (sys_mutex_lock(hdl->port_lock_tx_mtx, RELEASE_TIMEOUT) == ESUCC) {

                        sys_critical_section_begin();
                        {
                                sys_mutex_unlock(hdl->port_lock_rx_mtx);
                                sys_mutex_unlock(hdl->port_lock_tx_mtx);

                                sys_mutex_destroy(hdl->port_lock_rx_mtx);
                                sys_mutex_destroy(hdl->port_lock_tx_mtx);

                                sys_semaphore_destroy(hdl->data_write_sem);

                                UART_turn_off(&UART[hdl->major]);

                                UART_mem[hdl->major] = NULL;
                                sys_free(device_handle);
                        }
                        sys_critical_section_end();

                        return ESUCC;
                }

                sys_mutex_unlock(hdl->port_lock_rx_mtx);
        }

        return EBUSY;
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
API_MOD_OPEN(UART, void *device_handle, u32_t flags)
{
        UNUSED_ARG2(device_handle, flags);

        return ESUCC;
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
API_MOD_CLOSE(UART, void *device_handle, bool force)
{
        UNUSED_ARG2(device_handle, force);

        return ESUCC;
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
API_MOD_WRITE(UART,
              void             *device_handle,
              const u8_t       *src,
              size_t            count,
              fpos_t           *fpos,
              size_t           *wrcnt,
              struct vfs_fattr  fattr)
{
        UNUSED_ARG2(fpos, fattr);

        struct UART_mem *hdl = device_handle;

        int result = sys_mutex_lock(hdl->port_lock_tx_mtx, MTX_BLOCK_TIMEOUT);
        if (result == ESUCC) {
                hdl->Tx_buffer.src_ptr   = src;
                hdl->Tx_buffer.data_size = count;

                SET_BIT(UART[hdl->major].UART->CR1, USART_CR1_TXEIE);

                result = sys_semaphore_wait(hdl->data_write_sem, TX_WAIT_TIMEOUT);
                if (result == ESUCC) {
                        *wrcnt = count - hdl->Tx_buffer.data_size;
                }

                sys_mutex_unlock(hdl->port_lock_tx_mtx);
        }

        return result;
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
API_MOD_READ(UART,
             void            *device_handle,
             u8_t            *dst,
             size_t           count,
             fpos_t          *fpos,
             size_t          *rdcnt,
             struct vfs_fattr fattr)
{
        UNUSED_ARG2(fpos, fattr);

        struct UART_mem *hdl = device_handle;

        int result = sys_mutex_lock(hdl->port_lock_rx_mtx, MTX_BLOCK_TIMEOUT);
        if (result == ESUCC) {
                *rdcnt = 0;

                while (count--) {
                        result = sys_semaphore_wait(hdl->data_read_sem, RX_WAIT_TIMEOUT);
                        if (result == ESUCC) {
                                CLEAR_BIT(UART[hdl->major].UART->CR1, USART_CR1_RXNEIE);
                                if (FIFO_read(&hdl->Rx_FIFO, dst)) {
                                        dst++;
                                        (*rdcnt)++;
                                }
                                SET_BIT(UART[hdl->major].UART->CR1, USART_CR1_RXNEIE);
                        }
                }

                sys_mutex_unlock(hdl->port_lock_rx_mtx);
        }

        return result;
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
API_MOD_IOCTL(UART, void *device_handle, int request, void *arg)
{
        struct UART_mem *hdl    = device_handle;
        int              result = EINVAL;

        if (arg) {
                switch (request) {
                case IOCTL_UART__SET_CONFIGURATION:
                        UART_configure(hdl->major, arg);
                        hdl->config = *static_cast(struct UART_config *, arg);
                        result = ESUCC;
                        break;

                case IOCTL_UART__GET_CONFIGURATION:
                        *static_cast(struct UART_config *, arg) = hdl->config;
                        result = ESUCC;
                        break;

                case IOCTL_UART__GET_CHAR_UNBLOCKING:
                        if (!FIFO_read(&hdl->Rx_FIFO, arg)) {
                                result = EAGAIN;
                        } else {
                                result = ESUCC;
                        }
                        break;

                default:
                        result = EBADRQC;
                        break;
                }
        }

        return result;
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
API_MOD_FLUSH(UART, void *device_handle)
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
API_MOD_STAT(UART, void *device_handle, struct vfs_dev_stat *device_stat)
{
        struct UART_mem *hdl = device_handle;

        device_stat->st_size  = 0;
        device_stat->st_major = hdl->major;
        device_stat->st_minor = 0;

        return ESUCC;
}

//==============================================================================
/**
 * @brief Function enable USART clock
 *
 * @param[in] UART              UART registers
 *
 * @return One of errno value
 */
//==============================================================================
static int UART_turn_on(const UART_regs_t *UART)
{
        if (!(*UART->APBENR & UART->APBENR_UARTEN)) {
                SET_BIT(*UART->APBRSTR, UART->APBRSTR_UARTRST);
                CLEAR_BIT(*UART->APBRSTR, UART->APBRSTR_UARTRST);
                SET_BIT(*UART->APBENR, UART->APBENR_UARTEN);
                return ESUCC;
        } else {
                return EADDRINUSE;
        }
}

//==============================================================================
/**
 * @brief Function disable USART clock
 *
 * @param[in] UART              UART registers
 *
 * @return One of errno value.
 */
//==============================================================================
static int UART_turn_off(const UART_regs_t *UART)
{
        NVIC_DisableIRQ(UART->IRQn);
        SET_BIT(*UART->APBRSTR, UART->APBRSTR_UARTRST);
        CLEAR_BIT(*UART->APBRSTR, UART->APBRSTR_UARTRST);
        CLEAR_BIT(*UART->APBENR, UART->APBENR_UARTEN);
        return ESUCC;
}

//==============================================================================
/**
 * @brief Function configure selected UART
 *
 * @param major         major device number
 * @param config        configuration structure
 */
//==============================================================================
static void UART_configure(u8_t major, const struct UART_config *config)
{
        const UART_regs_t *DEV = &UART[major];

        /* set baud */
        RCC_ClocksTypeDef freq;
        RCC_GetClocksFreq(&freq);

        u32_t PCLK = (DEV->UART == USART1) ? freq.PCLK2_Frequency : freq.PCLK1_Frequency;

        DEV->UART->BRR = (PCLK / (config->baud)) + 1;

        /* set 8 bit word length and wake idle line */
        CLEAR_BIT(DEV->UART->CR1, USART_CR1_M | USART_CR1_WAKE);

        /* set parity */
        switch (config->parity) {
        case UART_PARITY__OFF:
                CLEAR_BIT(DEV->UART->CR1, USART_CR1_PCE);
                break;
        case UART_PARITY__EVEN:
                SET_BIT(DEV->UART->CR1, USART_CR1_PCE);
                CLEAR_BIT(DEV->UART->CR1, USART_CR1_PS);
                break;
        case UART_PARITY__ODD:
                SET_BIT(DEV->UART->CR1, USART_CR1_PCE);
                SET_BIT(DEV->UART->CR1, USART_CR1_PS);
                break;
        }

        /* transmitter enable */
        if (config->tx_enable) {
                SET_BIT(DEV->UART->CR1, USART_CR1_TE);
        } else {
                CLEAR_BIT(DEV->UART->CR1, USART_CR1_TE);
        }

        /* receiver enable */
        if (config->rx_enable) {
                SET_BIT(DEV->UART->CR1, USART_CR1_RE);
        } else {
                CLEAR_BIT(DEV->UART->CR1, USART_CR1_RE);
        }

        /* enable LIN if configured */
        if (config->LIN_mode_enable) {
                SET_BIT(DEV->UART->CR2, USART_CR2_LINEN);
        } else {
                CLEAR_BIT(DEV->UART->CR2, USART_CR2_LINEN);
        }

        /* configure stop bits */
        if (config->stop_bits == UART_STOP_BIT__1) {
                CLEAR_BIT(DEV->UART->CR2, USART_CR2_STOP);
        } else {
                CLEAR_BIT(DEV->UART->CR2, USART_CR2_STOP);
                SET_BIT(DEV->UART->CR2, USART_CR2_STOP_1);
        }

        /* clock configuration (synchronous mode) */
        CLEAR_BIT(DEV->UART->CR2, USART_CR2_CLKEN | USART_CR2_CPOL | USART_CR2_CPHA | USART_CR2_LBCL);

        /* LIN break detection length */
        if (config->LIN_break_length == UART_LIN_BREAK__10_BITS) {
                CLEAR_BIT(DEV->UART->CR2, USART_CR2_LBDL);
        } else {
                SET_BIT(DEV->UART->CR2, USART_CR2_LBDL);
        }

        /* hardware flow control */
        if (config->hardware_flow_ctrl) {
                SET_BIT(DEV->UART->CR3, USART_CR3_CTSE | USART_CR3_RTSE);
        } else {
                CLEAR_BIT(DEV->UART->CR3, USART_CR3_CTSE | USART_CR3_RTSE);
        }

        /* configure single wire mode */
        if (config->single_wire_mode) {
                SET_BIT(DEV->UART->CR3, USART_CR3_HDSEL);
        } else {
                CLEAR_BIT(DEV->UART->CR3, USART_CR3_HDSEL);
        }

        /* enable RXNE interrupt */
        SET_BIT(DEV->UART->CR1, USART_CR1_RXNEIE);

        /* enable UART */
        SET_BIT(DEV->UART->CR1, USART_CR1_UE);
}

//==============================================================================
/**
 * @brief Function write data to FIFO
 *
 * @param fifo          fifo buffer
 * @param data          data to write
 *
 * @return true if success, false on error
 */
//==============================================================================
static bool FIFO_write(struct Rx_FIFO *fifo, u8_t *data)
{
        if (fifo->buffer_level < _UART_RX_BUFFER_SIZE) {
                fifo->buffer[fifo->write_index++] = *data;

                if (fifo->write_index >= _UART_RX_BUFFER_SIZE) {
                        fifo->write_index = 0;
                }

                fifo->buffer_level++;

                return true;
        } else {
                return false;
        }
}

//==============================================================================
/**
 * @brief Function read data from FIFO
 *
 * @param fifo          fifo buffer
 * @param data          data result
 *
 * @return true if success, false on error
 */
//==============================================================================
static bool FIFO_read(struct Rx_FIFO *fifo, u8_t *data)
{
        if (fifo->buffer_level > 0) {
                *data = fifo->buffer[fifo->read_index++];

                if (fifo->read_index >= _UART_RX_BUFFER_SIZE) {
                        fifo->read_index = 0;
                }

                fifo->buffer_level--;

                return true;
        } else {
                return false;
        }
}

//==============================================================================
/**
 * @brief Interrupt handling
 *
 * @param major         major device number
 */
//==============================================================================
static void IRQ_handle(u8_t major)
{
        bool yield = false;

        const UART_regs_t *DEV = &UART[major];

        /* receiver interrupt handler */
        int received = 0;
        while ((DEV->UART->CR1 & USART_CR1_RXNEIE) && (DEV->UART->SR & (USART_SR_RXNE | USART_SR_ORE))) {
                u8_t DR = DEV->UART->DR;

                if (FIFO_write(&UART_mem[major]->Rx_FIFO, &DR)) {
                        received++;
                }
        }

        while (received--) {
                sys_semaphore_signal_from_ISR(UART_mem[major]->data_read_sem, NULL);
                yield = true;
        }


        /* transmitter interrupt handler */
        if ((DEV->UART->CR1 & USART_CR1_TXEIE) && (DEV->UART->SR & USART_SR_TXE)) {

                if (UART_mem[major]->Tx_buffer.data_size && UART_mem[major]->Tx_buffer.src_ptr) {
                        DEV->UART->DR = *(UART_mem[major]->Tx_buffer.src_ptr++);

                        if (--UART_mem[major]->Tx_buffer.data_size == 0) {
                                CLEAR_BIT(DEV->UART->SR, USART_SR_TC);
                                SET_BIT(DEV->UART->CR1, USART_CR1_TCIE);
                                CLEAR_BIT(DEV->UART->CR1, USART_CR1_TXEIE);
                                UART_mem[major]->Tx_buffer.src_ptr = NULL;
                        }
                } else {
                        /* this shall never happen */
                        CLEAR_BIT(DEV->UART->CR1, USART_CR1_TXEIE);
                        sys_semaphore_signal_from_ISR(UART_mem[major]->data_write_sem, NULL);
                }

        } else if ((DEV->UART->CR1 & USART_CR1_TCIE) && (DEV->UART->SR & USART_SR_TC)) {

                CLEAR_BIT(DEV->UART->CR1, USART_CR1_TCIE);
                sys_semaphore_signal_from_ISR(UART_mem[major]->data_write_sem, NULL);
                yield = true;
        }


        /* yield thread if data send or received */
        if (yield) {
                sys_thread_yield_from_ISR();
        }
}

//==============================================================================
/**
 * @brief USART1 Interrupt
 */
//==============================================================================
#if defined(RCC_APB2ENR_USART1EN)
void USART1_IRQHandler(void)
{
        IRQ_handle(_UART1);
}
#endif

//==============================================================================
/**
 * @brief USART2 Interrupt
 */
//==============================================================================
#if defined(RCC_APB1ENR_USART2EN)
void USART2_IRQHandler(void)
{
        IRQ_handle(_UART2);
}
#endif

//==============================================================================
/**
 * @brief USART3 Interrupt
 */
//==============================================================================
#if defined(RCC_APB1ENR_USART3EN)
void USART3_IRQHandler(void)
{
        IRQ_handle(_UART3);
}
#endif

//==============================================================================
/**
 * @brief UART4 Interrupt
 */
//==============================================================================
#if defined(RCC_APB1ENR_UART4EN)
void UART4_IRQHandler(void)
{
        IRQ_handle(_UART4);
}
#endif

//==============================================================================
/**
 * @brief UART5 Interrupt
 */
//==============================================================================
#if defined(RCC_APB1ENR_UART5EN)
void UART5_IRQHandler(void)
{
        IRQ_handle(_UART5);
}
#endif

/*==============================================================================
  End of file
==============================================================================*/
