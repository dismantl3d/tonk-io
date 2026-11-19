// Tonk.io authoritative server skeleton.
// TODO next: WebSocket layer (e.g. IXWebSocket), Firebase ID-token verification,
// snapshot broadcast, match lifecycle, result write-back to Firestore.
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "core/sim.h"

int main() {
  using namespace tonk;
  using clock = std::chrono::steady_clock;

  std::vector<Tank> tanks(1);
  tanks[0].id = 1;
  tanks[0].pos = {kMapSize / 2, kMapSize / 2};

  std::printf("Tonk.io server skeleton: %.0f Hz tick, headless demo (3 s)\n", kTickHz);
  auto next = clock::now();
  const auto tick = std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(kDt));

  for (int t = 0; t < (int)(3 * kTickHz); ++t) {
    Input in{1, 0, 0.0f, false};  // placeholder: later comes from network
    for (auto& tank : tanks) stepTank(tank, in);
    if (t % (int)kTickHz == 0) std::printf("t=%ds tank pos=(%.0f, %.0f)\n", t / (int)kTickHz, tanks[0].pos.x, tanks[0].pos.y);
    next += tick;
    std::this_thread::sleep_until(next);  // fixed timestep
  }
  return 0;
}
