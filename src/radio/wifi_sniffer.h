#pragma once
#include <cstdint>
// Wi-Fi promiscuous capture of management frames with channel hopping.
namespace wifi_sniffer {
void start();
void pin_channel(uint8_t ch);  // 0 = resume hopping
uint8_t channel();
uint32_t frames();  // management frames received since boot (self-test)
}
