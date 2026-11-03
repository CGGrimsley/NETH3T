#include "alert/audio.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "board_pins.h"

namespace {
constexpr int kRate = 16000;
constexpr i2s_port_t kPort = I2S_NUM_0;
volatile bool g_ok = false;
volatile bool g_chirp = false;
volatile uint16_t g_tone = 0, g_pulse_x10 = 0;
volatile uint8_t g_vol = 6;

void play(uint16_t hz, uint32_t ms) {
  int16_t buf[256];
  int16_t amp = (int16_t)(g_vol * 1200);
  uint32_t total = kRate * ms / 1000, done = 0;
  uint32_t half = hz ? kRate / (2 * hz) : 0;
  static uint32_t phase = 0;
  while (done < total) {
    size_t n = total - done < 256 ? total - done : 256;
    for (size_t i = 0; i < n; i++, phase++) buf[i] = (hz && ((phase / half) & 1)) ? amp : (hz ? -amp : 0);
    size_t written;
    i2s_write(kPort, buf, n * sizeof(int16_t), &written, portMAX_DELAY);
    done += n;
  }
}

void task(void*) {
  for (;;) {
    if (g_chirp) {
      g_chirp = false;
      play(1200, 80);
      play(1800, 80);
      continue;
    }
    uint16_t tone = g_tone, pulse = g_pulse_x10;  // read once: stop() may race
    if (tone && pulse) {
      uint32_t period = 10000 / pulse;  // ms
      play(tone, 30);
      play(0, period > 30 ? period - 30 : 0);
    } else {
      play(0, 20);  // keep DMA fed with silence
    }
  }
}
}  // namespace

namespace audio {
void init() {
  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, HIGH);  // enable amplifier
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = kRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.tx_desc_auto_clear = true;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCLK;
  pins.data_out_num = PIN_I2S_DATA;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) return;
  if (i2s_set_pin(kPort, &pins) != ESP_OK) return;
  g_ok = true;
  xTaskCreatePinnedToCore(task, "audio", 3072, nullptr, 1, nullptr, 0);
}
void chirp() { if (g_ok) g_chirp = true; }
void hunt_pulse(uint16_t tone_hz, uint16_t pulse_hz_x10) { g_tone = tone_hz; g_pulse_x10 = pulse_hz_x10; }
void stop() { g_tone = 0; g_pulse_x10 = 0; }
void set_volume(uint8_t v) { g_vol = v > 10 ? 10 : v; }
}  // namespace audio
