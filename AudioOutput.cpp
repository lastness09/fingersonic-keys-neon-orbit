// SPDX-License-Identifier: MIT
#include "AudioOutput.h"

bool AudioOutput::begin() {
  i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  channel.dma_desc_num = 4;
  channel.dma_frame_num = arp::BlockFrames;
  if (i2s_new_channel(&channel, &tx_, nullptr) != ESP_OK) return false;
  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(arp::SampleRate);
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = gpio_num_t(arp::BclkPin);
  config.gpio_cfg.ws = gpio_num_t(arp::WsPin);
  config.gpio_cfg.dout = gpio_num_t(arp::DataPin);
  config.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(tx_, &config) != ESP_OK || i2s_channel_enable(tx_) != ESP_OK) {
    i2s_del_channel(tx_);
    tx_ = nullptr;
    return false;
  }
  return true;
}

bool AudioOutput::write(const int16_t* stereo) {
  size_t written = 0;
  return tx_ && i2s_channel_write(tx_, stereo, arp::BlockFrames * 2 * sizeof(int16_t),
                                &written, pdMS_TO_TICKS(100)) == ESP_OK
             && written == arp::BlockFrames * 2 * sizeof(int16_t);
}
