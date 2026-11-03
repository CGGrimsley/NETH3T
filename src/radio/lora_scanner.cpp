#include "radio/lora_scanner.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <cstring>
#include "board_pins.h"
#include "core/db_task.h"
#include "core/health.h"
#include "core/spi_bus.h"
#include "detect/lora_headers.h"
#include "detect/noise_floor.h"
#include "settings/settings.h"

namespace {
LR1121* g_radio = nullptr;
bool g_ok = false;
char g_version[32] = "-";

// RF switch (LilyGO MVSRLora): DIO5/DIO6
const uint32_t kRfPins[] = {RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC};
const Module::RfSwitchMode_t kRfTable[] = {
    {LR11x0::MODE_STBY, {LOW, LOW}},  {LR11x0::MODE_RX, {HIGH, LOW}},
    {LR11x0::MODE_TX, {LOW, HIGH}},   {LR11x0::MODE_TX_HP, {LOW, HIGH}},
    {LR11x0::MODE_TX_HF, {LOW, LOW}}, {LR11x0::MODE_GNSS, {LOW, LOW}},
    {LR11x0::MODE_WIFI, {LOW, LOW}},  END_OF_MODE_TABLE,
};

constexpr uint32_t kCadBudgetMs = 700;   // per ELRS plan visit
constexpr uint32_t kCadTimeoutMs = 20;
constexpr uint8_t kMaxConsecErrors = 5;
constexpr uint32_t kSuspendMs = 30000;

SemaphoreHandle_t g_mx = nullptr;  // guards g_fp, g_rows, g_lock, g_stats
RcFingerprint g_fp;
lora_scanner::SweepRow g_rows[3];
bool g_row_valid[3] = {};
NoiseFloor g_nf[3];
lora_scanner::Stats g_stats = {};

struct LockState { bool on; uint32_t f; uint8_t sf; uint32_t bw; bool fhss; } g_lock = {};
volatile bool g_region_dirty = true;
volatile Region g_region = Region::FCC;
ScanPlan g_plans[kMaxPlans];
size_t g_np = 0;
uint8_t g_err_run[kMaxPlans] = {};
uint32_t g_suspend_until[kMaxPlans] = {};
const char* volatile g_current = "-";
volatile bool g_rx_flag = false;

struct Mx {
  Mx() { xSemaphoreTake(g_mx, portMAX_DELAY); }
  ~Mx() { xSemaphoreGive(g_mx); }
};

void IRAM_ATTR on_rx() { g_rx_flag = true; }

// ---- radio configuration cache ----
struct Cfg { uint32_t bw; uint8_t sf; uint8_t sync; bool valid; } g_cfg = {};

bool ok(int16_t st, size_t plan) {
  if (st == RADIOLIB_ERR_NONE) { g_err_run[plan] = 0; return true; }
  Mx m;
  g_stats.errors++;
  if (++g_err_run[plan] >= kMaxConsecErrors) {
    g_err_run[plan] = 0;
    g_suspend_until[plan] = millis() + kSuspendMs;
    g_stats.suspended++;
  }
  return false;
}

bool tune(size_t plan, uint32_t f_khz) {
  spi_bus::Guard g;
  return ok(g_radio->setFrequency(f_khz / 1000.0f), plan);
}

bool modem(size_t plan, uint32_t bw, uint8_t sf, bool hf, uint8_t sync) {
  if (g_cfg.valid && g_cfg.bw == bw && g_cfg.sf == sf && g_cfg.sync == sync) return true;
  spi_bus::Guard g;
  if (!ok(g_radio->setBandwidth(bw / 1000.0f, hf), plan)) return false;
  if (!ok(g_radio->setSpreadingFactor(sf), plan)) return false;
  if (!ok(g_radio->setSyncWord(sync), plan)) return false;
  g_cfg = {bw, sf, sync, true};
  return true;
}

uint8_t band_of(const ScanPlan& p) {
  if (p.hf) return 2;
  return p.freqs_khz[0] < 500000 ? 1 : 0;
}

// ---- modes ----
void run_sweep(size_t pi) {
  const ScanPlan& p = g_plans[pi];
  lora_scanner::SweepRow row = {};
  row.f0_khz = p.freqs_khz[0];
  row.step_khz = p.n_freqs > 1 ? p.freqs_khz[1] - p.freqs_khz[0] : 0;
  row.n = p.n_freqs;
  row.hf = p.hf;
  // Sweeps are done in LoRa mode; bandwidth just needs to be valid for the band.
  // Tune first: 2.4 GHz bandwidths are only accepted above 1 GHz.
  if (!tune(pi, p.freqs_khz[0])) return;
  if (!modem(pi, p.hf ? 812500 : 125000, 7, p.hf, 0x12)) return;
  for (uint8_t i = 0; i < p.n_freqs; i++) {
    spi_bus::Guard g;
    if (!ok(g_radio->setFrequency(p.freqs_khz[i] / 1000.0f), pi)) return;
    if (!ok(g_radio->startReceive(), pi)) return;
    delayMicroseconds(300);
    float r = g_radio->getRSSI(false, true);
    row.dbm[i] = (int8_t)constrain((int)r, -127, 0);
  }
  { spi_bus::Guard g; g_radio->standby(); }
  uint8_t b = band_of(p);
  uint32_t now = millis();
  row.t_ms = now;
  Mx m;
  g_nf[b].add_sweep(row.dbm, row.n);
  row.floor_dbm = g_nf[b].floor_dbm();
  for (uint8_t i = 0; i < row.n; i++)
    if (g_nf[b].hot(row.dbm[i])) g_fp.add_hot({p.freqs_khz[i], now, row.dbm[i]});
  g_rows[b] = row;
  g_row_valid[b] = true;
}

void run_cad(size_t pi, uint8_t sf, const uint32_t* freqs, uint8_t n) {
  const ScanPlan& p = g_plans[pi];
  if (!tune(pi, freqs[0])) return;  // set band before high-frequency bandwidths
  if (!modem(pi, p.bw_hz, sf, p.hf, 0x12)) return;
  uint32_t t0 = millis();
  while (millis() - t0 < kCadBudgetMs) {
    for (uint8_t i = 0; i < n; i++) {
      int16_t res;
      {
        spi_bus::Guard g;
        if (!ok(g_radio->setFrequency(freqs[i] / 1000.0f), pi)) return;
        if (!ok(g_radio->startChannelScan(), pi)) return;
        uint32_t ts = millis();
        while (!digitalRead(PIN_LR_IRQ) && millis() - ts < kCadTimeoutMs) taskYIELD();
        if (!digitalRead(PIN_LR_IRQ)) { g_radio->standby(); ok(RADIOLIB_ERR_RX_TIMEOUT, pi); continue; }
        res = g_radio->getChannelScanResult();
      }
      if (res == RADIOLIB_LORA_DETECTED) {
        float r;
        { spi_bus::Guard g; r = g_radio->getRSSI(false, false); }
        Mx m;
        g_fp.add_cad({freqs[i], sf, p.bw_hz, millis(), (int16_t)r});
        g_stats.cad_hits++;
      } else if (res != RADIOLIB_CHANNEL_FREE) {
        ok(res, pi);
      }
    }
  }
  spi_bus::Guard g;
  g_radio->standby();
}

void post_packet(const ScanPlan& p, uint32_t f, uint8_t sf, const uint8_t* buf, size_t len, float rssi, float snr) {
  Observation o;
  memset(&o, 0, sizeof o);
  o.t_ms = millis();
  o.freq_khz = f;
  o.rssi_dbm = (int16_t)rssi;
  o.snr_db = (int8_t)snr;
  o.lora = {sf, p.bw_hz, 0};
  MeshHeader mh;
  LwHeader lw;
  if (p.kind == PlanKind::MESHTASTIC && parse_meshtastic(buf, len, mh)) {
    o.type = SourceType::MESHTASTIC;
    o.conf = Confidence::DECODED;
    format_mesh_id(mh.sender, o.id);
    snprintf(o.label, sizeof o.label, "Mesh %s", o.id);
  } else if (p.kind == PlanKind::LORAWAN && parse_lorawan(buf, len, lw)) {
    o.type = SourceType::LORAWAN;
    o.conf = Confidence::DECODED;
    format_lw_id(lw, o.id);
    snprintf(o.label, sizeof o.label, "LW %s", o.id);
  } else {
    o.type = SourceType::LORA_UNKNOWN;
    o.conf = Confidence::HEURISTIC;
    snprintf(o.id, sizeof o.id, "LORA-%u-SF%u", (unsigned)(f / 1000), (unsigned)sf);
    snprintf(o.label, sizeof o.label, "LoRa %u.%03u SF%u", (unsigned)(f / 1000), (unsigned)(f % 1000), (unsigned)sf);
  }
  db::post(o);
}

void run_rx(size_t pi, uint32_t f, uint8_t sf) {
  const ScanPlan& p = g_plans[pi];
  if (!tune(pi, f)) return;
  if (!modem(pi, p.bw_hz, sf, p.hf, p.sync_word)) return;
  {
    spi_bus::Guard g;
    if (!ok(g_radio->explicitHeader(), pi)) return;
    if (!ok(g_radio->setPreambleLength(p.kind == PlanKind::MESHTASTIC ? 16 : 8), pi)) return;
    g_rx_flag = false;
    if (!ok(g_radio->startReceive(), pi)) return;
  }
  uint32_t t0 = millis();
  while (!g_rx_flag && millis() - t0 < p.rx_timeout_ms) vTaskDelay(pdMS_TO_TICKS(5));
  if (g_rx_flag) {
    uint8_t buf[256];
    size_t len;
    int16_t st;
    float rssi, snr;
    {
      spi_bus::Guard g;
      len = g_radio->getPacketLength();
      if (len > sizeof buf) len = sizeof buf;
      st = g_radio->readData(buf, len);
      rssi = g_radio->getRSSI();
      snr = g_radio->getSNR();
    }
    if (st == RADIOLIB_ERR_NONE) {
      { Mx m; g_stats.rx_pkts++; }
      post_packet(p, f, sf, buf, len, rssi, snr);
    }
  }
  spi_bus::Guard g;
  g_radio->standby();
}

void evaluate_fingerprints() {
  Observation out[6];
  size_t n;
  { Mx m; n = g_fp.evaluate(millis(), out, 6); }
  const Settings& s = settings::get();
  for (size_t i = 0; i < n; i++) {
    if (out[i].type == SourceType::ELRS && !s.en_elrs) continue;
    if (out[i].type == SourceType::FHSS && !s.en_fhss) continue;
    db::post(out[i]);
  }
}

// ---- scheduling ----
int find_plan(PlanKind k, bool hf, int band = -1) {
  for (size_t i = 0; i < g_np; i++)
    if (g_plans[i].kind == k && g_plans[i].hf == hf && (band < 0 || band_of(g_plans[i]) == band)) return (int)i;
  return -1;
}

bool usable(int pi) { return pi >= 0 && (int32_t)(millis() - g_suspend_until[pi]) >= 0; }

void run_plan_visit(int pi, uint32_t visit) {
  if (!usable(pi)) return;
  const ScanPlan& p = g_plans[pi];
  g_current = p.name;
  switch (p.mode) {
    case PlanMode::RSSI_SWEEP: run_sweep(pi); break;
    case PlanMode::CAD: run_cad(pi, p.sfs[visit % p.n_sfs], p.freqs_khz, p.n_freqs); break;
    case PlanMode::RX: {
      uint32_t combo = visit % (p.n_freqs * p.n_sfs);
      run_rx(pi, p.freqs_khz[combo % p.n_freqs], p.sfs[combo / p.n_freqs]);
      break;
    }
  }
}

void run_locked(const LockState& L, uint32_t visit) {
  if (L.fhss) {
    int pi = find_plan(PlanKind::SWEEP, false, L.f < 500000 ? 1 : 0);
    if (usable(pi)) { g_current = "LOCK sweep"; run_sweep(pi); }
    return;
  }
  if (L.bw == 500000 || L.bw == 812500) {
    int pi = find_plan(PlanKind::ELRS, L.bw == 812500);
    if (usable(pi)) { g_current = "LOCK ELRS"; run_cad(pi, L.sf, g_plans[pi].freqs_khz, g_plans[pi].n_freqs); }
    return;
  }
  for (size_t i = 0; i < g_np; i++) {
    const ScanPlan& p = g_plans[i];
    if (p.mode != PlanMode::RX) continue;
    for (uint8_t k = 0; k < p.n_freqs; k++) {
      if (p.freqs_khz[k] != L.f) continue;
      g_current = "LOCK RX";
      if (usable(i)) run_rx(i, L.f, L.sf ? L.sf : p.sfs[visit % p.n_sfs]);
      return;
    }
  }
  int pi = find_plan(PlanKind::SWEEP, L.f > 2000000, L.f > 2000000 ? 2 : (L.f < 500000 ? 1 : 0));
  if (usable(pi)) { g_current = "LOCK sweep"; run_sweep(pi); }
}

void scanner_task(void*) {
  uint32_t visit = 0, mesh_rot = 0;
  for (;;) {
    health::beat(health::LORA);
    if (g_region_dirty) {
      g_region_dirty = false;
      g_np = build_plans(g_region, g_plans, kMaxPlans);
      memset(g_err_run, 0, sizeof g_err_run);
      memset(g_suspend_until, 0, sizeof g_suspend_until);
      Mx m;
      for (NoiseFloor& nf : g_nf) nf = NoiseFloor();
      for (bool& v : g_row_valid) v = false;
    }
    LockState L;
    { Mx m; L = g_lock; }
    const Settings& s = settings::get();
    visit++;
    if (L.on) {
      run_locked(L, visit);
    } else {
      switch (visit % 7) {
        case 0: run_plan_visit(find_plan(PlanKind::SWEEP, false, 0), visit); break;
        case 1: if (s.en_elrs) run_plan_visit(find_plan(PlanKind::ELRS, false), visit / 7); break;
        case 2:
          if (s.en_mesh) {
            int idx[4], n = 0;
            for (size_t i = 0; i < g_np && n < 4; i++)
              if (g_plans[i].kind == PlanKind::MESHTASTIC) idx[n++] = (int)i;
            if (n) run_plan_visit(idx[mesh_rot++ % n], 0);
          }
          break;
        case 3: run_plan_visit(find_plan(PlanKind::SWEEP, true), visit); break;
        case 4: if (s.en_elrs) run_plan_visit(find_plan(PlanKind::ELRS, true), visit / 7); break;
        case 5: if (s.en_lorawan) run_plan_visit(find_plan(PlanKind::LORAWAN, false), visit / 7); break;
        case 6: run_plan_visit(find_plan(PlanKind::SWEEP, false, 1), visit); break;
      }
    }
    evaluate_fingerprints();
    vTaskDelay(1);
  }
}
}  // namespace

namespace lora_scanner {
bool init(int16_t* err) {
  g_mx = xSemaphoreCreateMutex();
  spi_bus::Guard g;
  g_radio = new LR1121(new Module(PIN_LR_CS, PIN_LR_IRQ, PIN_LR_RST, PIN_LR_BUSY, SPI));
  g_radio->setRfSwitchTable(kRfPins, kRfTable);
  int16_t st = g_radio->begin();
  if (err) *err = st;
  g_ok = st == RADIOLIB_ERR_NONE;
  if (!g_ok) return false;
  LR11x0VersionInfo_t vi;
  if (g_radio->getVersionInfo(&vi) == RADIOLIB_ERR_NONE)
    snprintf(g_version, sizeof g_version, "LR11x0 dev%02X hw%02X fw%u.%u", vi.device, vi.hardware,
             vi.fwMajor, vi.fwMinor);
  g_radio->setCRC(2);
  g_radio->setCodingRate(5);
  g_radio->setPacketReceivedAction(on_rx);
  g_radio->standby();
  return true;
}

void start() {
  if (!g_ok) return;
  g_region = settings::get().region;
  g_region_dirty = true;
  xTaskCreatePinnedToCore(scanner_task, "lora", 12288, nullptr, 2, nullptr, 1);
}

void set_region(Region r) {
  g_region = r;
  g_region_dirty = true;
}

void lock(uint32_t f, uint8_t sf, uint32_t bw, bool fhss) {
  if (!g_mx) return;
  Mx m;
  g_lock = {true, f, sf, bw, fhss};
}
void unlock() {
  if (!g_mx) return;
  Mx m;
  g_lock.on = false;
}

bool latest_sweep(uint8_t band, SweepRow& out) {
  if (!g_mx || band > 2) return false;
  Mx m;
  if (!g_row_valid[band]) return false;
  out = g_rows[band];
  return true;
}

Stats stats() {
  if (!g_mx) return {};
  Mx m;
  return g_stats;
}

size_t recent_cad(CadHit* out, size_t max) {
  if (!g_mx) return 0;
  Mx m;
  return g_fp.recent_cad(millis(), out, max);
}

const char* current_plan() { return g_ok ? g_current : "LORA OFF"; }
const char* version() { return g_version; }
}  // namespace lora_scanner
