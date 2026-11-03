#include "ui/ui.h"
#include "ui/ui_internal.h"

namespace ui_spectrum {
void draw(Arduino_Canvas* c) {
  c->setTextColor(ui_color::kGrey);
  c->setTextSize(2);
  c->setCursor(20, 200);
  c->print("Spectrum");
}
void input(const Input& in) {
  if (in.btn == Btn::BACK || in.swipe == 1) ui::go(Screen::TARGETS);
}
}  // namespace ui_spectrum
