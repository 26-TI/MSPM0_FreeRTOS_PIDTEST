/**
 * @file bsp_uart.hpp
 * @brief MSPM0 DMA 串口驱动 — 流缓冲区 + 双 DMA 全中断
 */

#ifndef __BSP_UART_MSPM0_HPP__
#define __BSP_UART_MSPM0_HPP__

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "ti_msp_dl_config.h"

template <size_t BUF_SIZE = 256>
class BspUart
{
public:
  struct Config
  {
    UART_Regs *uart;
    IRQn_Type  irq;
    uint8_t    dma_rx_chan;
    uint8_t    dma_tx_chan;
    uint8_t    dma_rx_trigger;
    uint8_t    dma_tx_trigger;
  };

  BspUart();
  ~BspUart();

  bool init(const Config &cfg);
  int  send(const uint8_t *data, size_t size, uint32_t timeout = portMAX_DELAY);
  int  receive(uint8_t *buf, size_t size, uint32_t timeout = portMAX_DELAY);
  void handle_isr(BaseType_t *woken);

  UART_Regs      *get_uart() const { return _cfg.uart; }
  static BspUart *find(UART_Regs *uart);

private:
  void tx_kickoff();

  Config               _cfg{};
  StreamBufferHandle_t _rx_stream = nullptr;
  StreamBufferHandle_t _tx_stream = nullptr;
  volatile uint8_t     _rx_byte __attribute__((aligned(4))){};
  uint8_t              _tx_buf[BUF_SIZE] __attribute__((aligned(4))){};

  static constexpr size_t MAX_INST = 4;
  static BspUart         *_instances[MAX_INST];
  static size_t           _inst_count;
};

#endif // __BSP_UART_MSPM0_HPP__
