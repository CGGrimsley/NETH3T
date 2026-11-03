// Landscape spectrum analyzer (spec 2026-11-03-selftest-and-spectrum §3).
// The UI router rotates the canvas and the touch input before calling in here,
// so everything below uses landscape coordinates (480 x 222).
#include <Arduino.h>
#include <cstring>
#include "core/power.h"
#include "radio/lora_scanner.h"
#include "settings/settings.h"
#include "ui/spectrum_view.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
constexpr int W = 480;
constexpr int kBarH = 22, kTraceY = 22, kTraceH = 128, kAxisY = 150, kWfY = 162, kWfRows = 60;
constexpr int kDbTop = -30, kDbBottom = -130;
struct TopBtn { int x0, x1; const char* label; };
const TopBtn kBand = {250, 310, "Band"}, kMinus = {316, 356, "-"}, kPlus = {362, 402, "+"}, kBack = {410, 476, "Back"};

uint8_t g_band = 0;
Region g_region = Region::FCC;
Span g_span[3];
bool g_span_init = false;
bool g_marker = false;
uint32_t g_marker_f = 0;
PeakHold g_peak;
int8_t g_wf[kWfRows][80];
uint8_t g_wf_n = 0, g_wf_head = 0, g_wf_len = 0;
int16_t g_wf_floor[kWfRows];
uint32_t g_last_t = 0;
lora_scanner::SweepRow g_row = {};
bool g_have_row = false;

BandLimits limits() { return band_limits(g_band, g_region); }
uint32_t min_step() { return g_band == 2 ? 200 : 50; }

void reset_view() {
  g_wf_n = g_wf_head = g_wf_len = 0;
  g_peak.reset();
  g_have_row = false;
  g_last_t = 0;
}

void apply_span() {
  reset_view();
  lora_scanner::set_focus(g_band, g_span[g_band].f0_khz, g_span[g_band].f1_khz);
}

void init_spans() {
  g_region = settings::get().region;
  for (uint8_t b = 0; b < 3; b++) g_span[b] = full_span(band_limits(b, g_region));
  g_span_init = true;
}

int dbm_to_y(int dbm) {
  if (dbm > kDbTop) dbm = kDbTop;
  if (dbm < kDbBottom) dbm = kDbBottom;
  return kTraceY + (kDbTop - dbm) * (kTraceH - 1) / (kDbTop - kDbBottom);
}

uint16_t heat(int8_t dbm, int16_t floor) {
  float t = (dbm - floor) / 40.0f;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  if (t < 0.25f) return rgb(0, 0, (uint8_t)(t * 4 * 200));
  if (t < 0.5f) return rgb(0, (uint8_t)((t - 0.25f) * 4 * 220), (uint8_t)(200 - (t - 0.25f) * 4 * 200));
  if (t < 0.75f) return rgb((uint8_t)((t - 0.5f) * 4 * 255), 220, 0);
  return rgb(255, (uint8_t)(220 - (t - 0.75f) * 4 * 220), 0);
}

// Only rows that match the viewed range are used; normal full-band sweeps of the
// same band are ignored while zoomed.
void ingest() {
  lora_scanner::SweepRow r;
  if (!lora_scanner::latest_sweep(g_band, r) || r.t_ms == g_last_t) return;
  const Span& s = g_span[g_band];
  ScanPlan want = make_sweep_plan("", s.f0_khz, s.f1_khz, min_step(), g_band == 2);
  if (r.f0_khz != s.f0_khz || r.n != want.n_freqs) return;
  g_last_t = r.t_ms;
  g_row = r;
  g_have_row = true;
  g_peak.update(r.dbm, r.n, r.t_ms);
  if (r.n != g_wf_n) { g_wf_n = r.n; g_wf_head = g_wf_len = 0; }
  memcpy(g_wf[g_wf_head], r.dbm, r.n);
  g_wf_floor[g_wf_head] = r.floor_dbm;
  g_wf_head = (uint8_t)((g_wf_head + 1) % kWfRows);
  if (g_wf_len < kWfRows) g_wf_len++;
}

int bin_x(uint8_t i, uint8_t n) { return n > 1 ? i * (W - 1) / (n - 1) : 0; }

void fmt_mhz(uint32_t khz, char* out, size_t cap, bool fine) {
  if (fine) snprintf(out, cap, "%u.%03u", (unsigned)(khz / 1000), (unsigned)(khz % 1000));
  else snprintf(out, cap, "%u.%u", (unsigned)(khz / 1000), (unsigned)(khz % 1000 / 100));
}

void draw_button(Arduino_Canvas* c, const TopBtn& b) {
  c->fillRoundRect(b.x0, 1, b.x1 - b.x0, kBarH - 2, 4, kPanel);
  c->drawRoundRect(b.x0, 1, b.x1 - b.x0, kBarH - 2, 4, kGrey);
  c->setTextSize(1);
  c->setTextColor(kText);
  int tw = (int)strlen(b.label) * 6;
  c->setCursor(b.x0 + (b.x1 - b.x0 - tw) / 2, 8);
  c->print(b.label);
}

bool hit(const TopBtn& b, int16_t x, int16_t y) { return y < kBarH + 4 && x >= b.x0 - 3 && x < b.x1 + 3; }

const char* band_name() {
  if (g_band == 1) return "433";
  if (g_band == 2) return "2.4G";
  return g_region == Region::FCC ? "915" : "868";
}

void change_band() {
  g_band = (uint8_t)((g_band + 1) % 3);
  g_marker = false;
  apply_span();
}

void zoom(bool in) {
  const Span& s = g_span[g_band];
  uint32_t centre = g_marker ? g_marker_f : (s.f0_khz + s.f1_khz) / 2;
  Span n = in ? zoom_in(s, centre, limits()) : zoom_out(s, centre, limits());
  if (n.f0_khz == s.f0_khz && n.f1_khz == s.f1_khz) return;
  g_span[g_band] = n;
  if (g_marker && (g_marker_f < n.f0_khz || g_marker_f > n.f1_khz)) g_marker = false;
  apply_span();
}
}  // namespace

namespace ui_spectrum {
void enter() {
  if (!g_span_init || g_region != settings::get().region) init_spans();
  apply_span();
}

void leave() { lora_scanner::clear_focus(); }

void draw(Arduino_Canvas* c) {
  if (g_region != settings::get().region) enter();  // region changed elsewhere
  ingest();
  const Span& s = g_span[g_band];
  uint32_t span = s.f1_khz - s.f0_khz;
  bool fine = span < 10000;

  // Top bar
  c->fillRect(0, 0, W, kBarH, kPanel);
  c->setTextSize(1);
  c->setTextColor(kAmber);
  c->setCursor(4, 4);
  c->printf("%s MHz", band_name());
  c->setTextColor(kText);
  c->setCursor(4, 13);
  if (g_have_row) c->printf("span %lu.%lu MHz  step %lu kHz  floor %d", (unsigned long)(span / 1000),
                            (unsigned long)(span % 1000 / 100), (unsigned long)g_row.step_khz, g_row.floor_dbm);
  else c->print("waiting for sweep...");
  int8_t pct = power::battery_pct();
  c->setCursor(196, 4);
  c->setTextColor(kGrey);
  if (pct >= 0) c->printf("%d%%", pct);
  draw_button(c, kBand);
  draw_button(c, kMinus);
  draw_button(c, kPlus);
  draw_button(c, kBack);

  if (!lora_scanner::ok()) {
    c->setTextSize(2);
    c->setTextColor(kRed);
    c->setCursor(90, 90);
    c->print("LoRa radio not available");
    return;
  }

  // Grid
  c->setTextSize(1);
  for (int db = kDbTop; db >= kDbBottom; db -= 10) {
    int y = dbm_to_y(db);
    for (int x = 0; x < W; x += 4) c->drawPixel(x, y, kDim);
    if ((db / 10) % 2 == 0) {
      c->setTextColor(kGrey);
      c->setCursor(2, y + 2 > kTraceY + kTraceH - 8 ? y - 9 : y + 2);
      c->printf("%d", db);
    }
  }

  if (g_have_row) {
    const lora_scanner::SweepRow& r = g_row;
    // Noise floor
    c->drawFastHLine(0, dbm_to_y(r.floor_dbm), W, kBlue);
    // Peak hold and live trace
    for (uint8_t i = 1; i < r.n; i++) {
      c->drawLine(bin_x(i - 1, r.n), dbm_to_y(g_peak.at(i - 1)), bin_x(i, r.n), dbm_to_y(g_peak.at(i)), kRed);
      c->drawLine(bin_x(i - 1, r.n), dbm_to_y(r.dbm[i - 1]), bin_x(i, r.n), dbm_to_y(r.dbm[i]), kAmber);
    }
    // CAD hits in view
    CadHit hits[16];
    size_t nh = lora_scanner::recent_cad(hits, 16);
    for (size_t i = 0; i < nh; i++) {
      if (hits[i].freq_khz < s.f0_khz || hits[i].freq_khz > s.f1_khz) continue;
      int x = freq_to_x(hits[i].freq_khz, s, W);
      c->drawFastVLine(x, kTraceY, 10, kText);
      c->setTextColor(kText);
      c->setCursor(constrain(x + 2, 0, W - 20), kTraceY + 2);
      c->printf("SF%u", hits[i].sf);
    }
    // Marker readout
    if (g_marker) {
      int x = freq_to_x(g_marker_f, s, W);
      c->drawFastVLine(x, kTraceY, kTraceH, kGreen);
      uint8_t bin = (uint8_t)((uint64_t)(g_marker_f - s.f0_khz) * (r.n - 1) / (span ? span : 1));
      if (bin >= r.n) bin = r.n - 1;
      char f[16];
      fmt_mhz(g_marker_f, f, sizeof f, true);
      c->fillRect(30, kTraceY + 12, 200, 11, kBg);
      c->setTextColor(kGreen);
      c->setCursor(32, kTraceY + 14);
      c->printf("%s MHz  %d dBm  (peak %d)", f, r.dbm[bin], g_peak.at(bin));
    }
  }

  // Frequency axis
  char a[16], m[16], b[16];
  fmt_mhz(s.f0_khz, a, sizeof a, fine);
  fmt_mhz((s.f0_khz + s.f1_khz) / 2, m, sizeof m, fine);
  fmt_mhz(s.f1_khz, b, sizeof b, fine);
  c->setTextColor(kGrey);
  c->setCursor(2, kAxisY + 2);
  c->print(a);
  c->setCursor(W / 2 - (int)strlen(m) * 3, kAxisY + 2);
  c->print(m);
  c->setCursor(W - 2 - (int)strlen(b) * 6, kAxisY + 2);
  c->print(b);

  // Waterfall, newest on top
  if (g_wf_len && g_wf_n) {
    int cw = W / g_wf_n;
    int x0 = (W - cw * g_wf_n) / 2;
    for (int k = 0; k < g_wf_len; k++) {
      int ri = (g_wf_head + kWfRows - 1 - k) % kWfRows;
      for (int i = 0; i < g_wf_n; i++) c->fillRect(x0 + i * cw, kWfY + k, cw, 1, heat(g_wf[ri][i], g_wf_floor[ri]));
    }
  }
}

void input(const Input& in) {
  if (in.btn == Btn::BACK || in.swipe != 0) { ui::go(Screen::TARGETS); return; }
  if (in.btn == Btn::UP) zoom(true);
  if (in.btn == Btn::DOWN) zoom(false);
  if (in.btn == Btn::DOWN_LONG) change_band();
  if (!in.tap) return;
  if (hit(kBack, in.x, in.y)) { ui::go(Screen::TARGETS); return; }
  if (hit(kBand, in.x, in.y)) { change_band(); return; }
  if (hit(kMinus, in.x, in.y)) { zoom(false); return; }
  if (hit(kPlus, in.x, in.y)) { zoom(true); return; }
  if (in.y >= kTraceY && in.y < kTraceY + kTraceH) {
    g_marker = true;
    g_marker_f = x_to_freq(in.x, g_span[g_band], W);
  }
}
}  // namespace ui_spectrum
