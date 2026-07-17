/**
 * @file bsp_uart.cpp
 * @brief MSPM0 版 BspUart — 模板实现 + UART ISR
 */

#include "bsp_uart.hpp"

/* 裸机 C++ 所需桩 */
/* 裸机 C++ 全局析构所需桩 */
void *__dso_handle = nullptr;

/* ==================== 静态成员定义 ==================== */
template <size_t BUF_SIZE>
BspUart<BUF_SIZE> *BspUart<BUF_SIZE>::_instances[MAX_INST] = {};

template <size_t BUF_SIZE>
size_t BspUart<BUF_SIZE>::_inst_count = 0;

/* ==================== 构造函数 ==================== */
template <size_t BUF_SIZE>
BspUart<BUF_SIZE>::BspUart()
{
  if (_inst_count < MAX_INST)
  {
    _instances[_inst_count++] = this;
  }
}

/* ==================== 析构 ==================== */
template <size_t BUF_SIZE>
BspUart<BUF_SIZE>::~BspUart()
{
  if (_rx_stream)
    vStreamBufferDelete(_rx_stream);
  if (_tx_stream)
    vStreamBufferDelete(_tx_stream);
}

/* ==================== 查找实例 ==================== */
template <size_t BUF_SIZE>
BspUart<BUF_SIZE> *BspUart<BUF_SIZE>::find(UART_Regs *uart)
{
  for (size_t i = 0; i < _inst_count; i++)
  {
    if (_instances[i] && _instances[i]->get_uart() == uart)
      return _instances[i];
  }
  return nullptr;
}

/* ==================== 初始化 ==================== */
template <size_t BUF_SIZE>
bool BspUart<BUF_SIZE>::init(const Config &cfg)
{
  _cfg = cfg;

  _rx_stream = xStreamBufferCreate(BUF_SIZE * 2, 1);
  _tx_stream = xStreamBufferCreate(BUF_SIZE * 2, 1);
  if (!_rx_stream || !_tx_stream)
    return false;

  NVIC_EnableIRQ(_cfg.irq);

  DL_UART_Main_enableFIFOs(_cfg.uart);
  DL_UART_Main_setRXFIFOThreshold(_cfg.uart, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);

  DL_DMA_disableChannel(DMA, _cfg.dma_rx_chan);
  DL_DMA_setSrcAddr(DMA, _cfg.dma_rx_chan, (uint32_t)&(_cfg.uart->RXDATA));
  DL_DMA_setDestAddr(DMA, _cfg.dma_rx_chan, (uint32_t)&_rx_byte);
  DL_DMA_setTransferSize(DMA, _cfg.dma_rx_chan, 1);
  DL_DMA_enableChannel(DMA, _cfg.dma_rx_chan);

  DL_DMA_disableChannel(DMA, _cfg.dma_tx_chan);
  DL_DMA_setSrcAddr(DMA, _cfg.dma_tx_chan, (uint32_t)_tx_buf);
  DL_DMA_setDestAddr(DMA, _cfg.dma_tx_chan, (uint32_t)&(_cfg.uart->TXDATA));

  return true;
}

/* ==================== ISR 处理 ==================== */
template <size_t BUF_SIZE>
void BspUart<BUF_SIZE>::handle_isr(BaseType_t *woken)
{
  DL_UART_IIDX iidx = DL_UART_Main_getPendingInterrupt(_cfg.uart);
  size_t       n;

  switch (iidx)
  {
    case DL_UART_MAIN_IIDX_DMA_DONE_RX:
      xStreamBufferSendFromISR(_rx_stream, (void *)&_rx_byte, 1, woken);
      DL_DMA_disableChannel(DMA, _cfg.dma_rx_chan);
      DL_DMA_setDestAddr(DMA, _cfg.dma_rx_chan, (uint32_t)&_rx_byte);
      DL_DMA_setTransferSize(DMA, _cfg.dma_rx_chan, 1);
      DL_DMA_enableChannel(DMA, _cfg.dma_rx_chan);
      break;

    case DL_UART_MAIN_IIDX_DMA_DONE_TX:
      n = xStreamBufferReceiveFromISR(_tx_stream, _tx_buf, BUF_SIZE, woken);
      if (n)
      {
        DL_DMA_disableChannel(DMA, _cfg.dma_tx_chan);
        DL_DMA_setSrcAddr(DMA, _cfg.dma_tx_chan, (uint32_t)_tx_buf);
        DL_DMA_setTransferSize(DMA, _cfg.dma_tx_chan, n);
        DL_DMA_enableChannel(DMA, _cfg.dma_tx_chan);
      }
      break;

    default:
      break;
  }
}

/* ==================== 发送 ==================== */
template <size_t BUF_SIZE>
int BspUart<BUF_SIZE>::send(const uint8_t *data, size_t size, uint32_t timeout)
{
  size_t n = xStreamBufferSend(_tx_stream, data, size, timeout);
  if (n > 0)
    tx_kickoff();
  return n;
}

/* ==================== 接收 ==================== */
template <size_t BUF_SIZE>
int BspUart<BUF_SIZE>::receive(uint8_t *buf, size_t size, uint32_t timeout)
{
  return xStreamBufferReceive(_rx_stream, buf, size, timeout);
}

/* ==================== TX 启动 ==================== */
template <size_t BUF_SIZE>
void BspUart<BUF_SIZE>::tx_kickoff()
{
  if (DL_DMA_isChannelEnabled(DMA, _cfg.dma_tx_chan))
    return;

  size_t n = xStreamBufferReceive(_tx_stream, _tx_buf, BUF_SIZE, 0);
  if (n)
  {
    DL_DMA_disableChannel(DMA, _cfg.dma_tx_chan);
    DL_DMA_setSrcAddr(DMA, _cfg.dma_tx_chan, (uint32_t)_tx_buf);
    DL_DMA_setTransferSize(DMA, _cfg.dma_tx_chan, n);
    DL_DMA_enableChannel(DMA, _cfg.dma_tx_chan);
  }
}

/* ==================== 模板显式实例化 ==================== */
template class BspUart<64>;
template class BspUart<128>;
template class BspUart<256>;

/* ==================== UART ISR ==================== */
extern "C" void UART0_IRQHandler(void)
{
  BaseType_t woken = pdFALSE;
  auto      *inst  = BspUart<64>::find(UART0);
  if (inst)
    inst->handle_isr(&woken);
  portYIELD_FROM_ISR(woken);
}

extern "C" void UART1_IRQHandler(void)
{
  BaseType_t woken = pdFALSE;
  auto      *inst  = BspUart<64>::find(UART1);
  if (inst)
    inst->handle_isr(&woken);
  portYIELD_FROM_ISR(woken);
}

extern "C" void UART2_IRQHandler(void)
{
  BaseType_t woken = pdFALSE;
  auto      *inst  = BspUart<64>::find(UART2);
  if (inst)
    inst->handle_isr(&woken);
  portYIELD_FROM_ISR(woken);
}

extern "C" void UART3_IRQHandler(void)
{
  BaseType_t woken = pdFALSE;
  auto      *inst  = BspUart<64>::find(UART3);
  if (inst)
    inst->handle_isr(&woken);
  portYIELD_FROM_ISR(woken);
}
