/**
 * @file main.cpp
 * @brief DMA 串口回环 — 使用 BspUart 类
 */

#include "main.hpp"

/* ---- 全局 UART0 实例 ---- */
static BspUart<64> bsp_uart0;

/* ========================================================================== */
extern "C" void dmaLoopbackTask(void *arg)
{
  uint8_t  buf[64];

  while (1)
  {
    size_t n = bsp_uart0.receive(&buf[0], 64, portMAX_DELAY);
    bsp_uart0.send(buf, n);
  }
}

/* ========================================================================== */
extern "C" void blinkTask(void *arg)
{
  while (1)
  {
    DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_22);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

int main()
{
  SYSCFG_DL_init();

  bsp_uart0.init({
    .uart           = UART0,
    .irq            = UART0_INT_IRQn,
    .dma_rx_chan    = DMA_CH1_CHAN_ID,
    .dma_tx_chan    = DMA_CH0_CHAN_ID,
    .dma_rx_trigger = UART0_INST_DMA_TRIGGER_0,
    .dma_tx_trigger = UART0_INST_DMA_TRIGGER_1,
  });

  TaskHandle_t h1, h2;
  xTaskCreate(blinkTask, "blink", 0x80, NULL, configMAX_PRIORITIES - 1, &h1);
  xTaskCreate(dmaLoopbackTask, "dmaLoop", 0x200, NULL, configMAX_PRIORITIES - 1, &h2);
  vTaskStartScheduler();
}
