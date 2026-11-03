#include <Arduino.h>
#include "selftest/selftest.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
constexpr int kRowH = 22, kListY = kStatusH + 44;  // 18 rows end at y 460

uint16_t color_of(StResult r) {
  switch (r) {
    case StResult::PASS: return kGreen;
    case StResult::WARN: return kAmber;
    case StResult::FAIL: return kRed;
    case StResult::CHECK: return kBlue;
    default: return kDim;
  }
}
}  // namespace

namespace ui_selftest {
void draw(Arduino_Canvas* c) {
  StItem it[selftest::kItems];
  size_t n = selftest::items(it, selftest::kItems);
  int pass = 0, warn = 0, fail = 0;
  for (size_t i = 0; i < n; i++) {
    if (it[i].r == StResult::PASS) pass++;
    if (it[i].r == StResult::WARN) warn++;
    if (it[i].r == StResult::FAIL) fail++;
  }
  c->setTextSize(2);
  c->setTextColor(kText);
  c->setCursor(6, kStatusH + 6);
  c->print("Self-test");
  draw_back(c);
  c->setTextSize(1);
  c->setTextColor(selftest::running() ? kAmber : kGrey);
  c->setCursor(6, kStatusH + 28);
  if (selftest::running()) c->printf("Running: %s", selftest::progress());
  else c->printf("Done  pass %d  warn %d  fail %d", pass, warn, fail);
  for (size_t i = 0; i < n; i++) {
    int y = kListY + (int)i * kRowH;
    uint16_t col = color_of(it[i].r);
    c->fillRoundRect(4, y, 40, 18, 3, col);
    c->setTextColor(it[i].r == StResult::PENDING ? kGrey : BLACK);
    c->setCursor(8, y + 5);
    c->print(selftest::result_name(it[i].r));
    c->setTextColor(kText);
    c->setCursor(50, y + 1);
    c->printf("%s %s", it[i].id, it[i].name);
    c->setTextColor(kGrey);
    c->setCursor(50, y + 11);
    c->printf("%.28s", it[i].detail);
  }
}

void input(const Input& in) {
  if (back_pressed(in)) ui::go(Screen::TARGETS);
}
}  // namespace ui_selftest
