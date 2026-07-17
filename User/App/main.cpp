/**
 * @file main.cpp
 * @brief DMA 串口回环 — 流缓冲区 + 双 DMA 全中断
 */

#include "main.hpp"
#include "stream_buffer.h"
#include <string.h>

#define CHUNK 256U
#define STREAM 1024U

static StreamBufferHandle_t g_rx_stream, g_tx_stream;
static volatile uint8_t     g_rx_byte __attribute__((aligned(4)));
static uint8_t              g_tx_buf[CHUNK] __attribute__((aligned(4)));

/* ========================================================================== */
extern "C" void UART0_IRQHandler(void)
{
  DL_UART_IIDX iidx  = DL_UART_Main_getPendingInterrupt(UART0_INST);
  BaseType_t   woken = pdFALSE;
  size_t       n;

  switch (iidx)
  {
    case DL_UART_MAIN_IIDX_DMA_DONE_RX:
      xStreamBufferSendFromISR(g_rx_stream, (void *)&g_rx_byte, 1, &woken);
      DL_DMA_disableChannel(DMA, DMA_CH1_CHAN_ID);
      DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&g_rx_byte);
      DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, 1);
      DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
      break;

    case DL_UART_MAIN_IIDX_DMA_DONE_TX:
      n = xStreamBufferReceiveFromISR(g_tx_stream, g_tx_buf, CHUNK, &woken);
      if (n)
      {
        DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
        DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)g_tx_buf);
        DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, n);
        DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
      }
      break;

    default:
      break;
  }
  portYIELD_FROM_ISR(woken);
}

/* ========================================================================== */
extern "C" void DMA_IRQHandler(void)
{
}

static void init(void)
{
  g_rx_stream = xStreamBufferCreate(STREAM, 1);
  g_tx_stream = xStreamBufferCreate(STREAM, 1);
  NVIC_EnableIRQ(UART0_INT_IRQn);

  /* FIFO 开启但阈值=1 entry：有 1 字节就触发 DMA，无卡顿 */
  DL_UART_Main_enableFIFOs(UART0_INST);
  DL_UART_Main_setRXFIFOThreshold(UART0_INST,
                                  DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);

  /* DMA CH1 (RX): 固定目的地址，每次收 1 字节 */
  DL_DMA_disableChannel(DMA, DMA_CH1_CHAN_ID);
  DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&(UART0->RXDATA));
  DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&g_rx_byte);
  DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, 1);
  DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);

  /* DMA CH0 (TX): 发送时由 ISR / kickoff 启动 */
  DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)g_tx_buf);
  DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&(UART0->TXDATA));
}

static void tx_kickoff(void)
{
  if (DL_DMA_isChannelEnabled(DMA, DMA_CH0_CHAN_ID))
    return;
  size_t n = xStreamBufferReceive(g_tx_stream, g_tx_buf, CHUNK, 0);
  if (n)
  {
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)g_tx_buf);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, n);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
  }
}

/* ========================================================================== */
extern "C" void dmaLoopbackTask(void *arg)
{
  uint8_t  buf[CHUNK];
  uint16_t total = 0;

  while (1)
  {
    size_t n = xStreamBufferReceive(g_rx_stream, &buf[total],
                                    CHUNK - total, pdMS_TO_TICKS(2));
    if (n)
    {
      total += n;
      if (total < CHUNK)
        continue;
    }

    if (total)
    {
      xStreamBufferSend(g_tx_stream, buf, total, portMAX_DELAY);
      tx_kickoff();
      total = 0;
    }
  }
}

/* ========================================================================== */
extern "C" void blinkTask(void *arg)
{
  while (1)
  {
    DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_22);
    vTaskDelay(500);
  }
}

int main()
{
  SYSCFG_DL_init();
  init();
  TaskHandle_t h1, h2;
  xTaskCreate(blinkTask, "blink", 0x80, NULL, configMAX_PRIORITIES - 1, &h1);
  xTaskCreate(dmaLoopbackTask, "dmaLoop", 0x200, NULL, configMAX_PRIORITIES - 1, &h2);
  vTaskStartScheduler();
}
