#include "taskbench_graph.h"

#include <cassert>
#include <stdexcept>
#include <vector>

namespace tb = oox_bench::taskbench;

int main() {
  tb::Config cfg;
  cfg.height = 3;
  cfg.width = 4;
  cfg.graphs = 1;
  cfg.pattern = tb::Pattern::Stencil;
  tb::Graph stencil(cfg);
  assert((stencil.deps(1, 0) == std::vector<int>{0, 1}));
  assert((stencil.deps(1, 1) == std::vector<int>{0, 1, 2}));
  assert((stencil.deps(1, 3) == std::vector<int>{2, 3}));
  assert(stencil.edge_count() == 20);

  cfg.pattern = tb::Pattern::Sweep;
  tb::Graph sweep(cfg);
  assert((sweep.deps(1, 0) == std::vector<int>{0}));
  assert((sweep.deps(1, 2) == std::vector<int>{1, 2}));

  cfg.pattern = tb::Pattern::Nearest;
  cfg.radix = 3;
  tb::Graph nearest(cfg);
  assert((nearest.deps(1, 2) == std::vector<int>{1, 2, 3}));

  cfg.pattern = tb::Pattern::Trivial;
  tb::Graph trivial(cfg);
  assert(trivial.deps(2, 2).empty());
  assert(trivial.edge_count() == 0);

  cfg.pattern = tb::Pattern::Random;
  cfg.radix = 2;
  cfg.seed = 42;
  tb::Graph random_a(cfg);
  tb::Graph random_b(cfg);
  assert(random_a.deps(2, 1) == random_b.deps(2, 1));

  // FFT width=1 must be rejected by config validation.
  cfg.pattern = tb::Pattern::FFT;
  cfg.width = 1;
  bool fft_width1_rejected = false;
  try {
    tb::Graph bad_fft(cfg);
    (void)bad_fft;
  } catch (const std::invalid_argument&) {
    fft_width1_rejected = true;
  }
  assert(fft_width1_rejected);

  // Across all patterns and moderate widths, every dependency index must stay in-range.
  for (tb::Pattern pattern :
       {tb::Pattern::Stencil, tb::Pattern::Sweep, tb::Pattern::Nearest, tb::Pattern::Spread,
        tb::Pattern::FFT, tb::Pattern::Tree, tb::Pattern::Random}) {
    for (int width = 1; width <= 32; ++width) {
      // FFT has explicit width constraints.
      if (pattern == tb::Pattern::FFT && ((width & (width - 1)) != 0 || width < 2)) {
        continue;
      }
      cfg.pattern = pattern;
      cfg.width = width;
      cfg.height = 16;
      cfg.radix = 7;
      cfg.seed = 123;
      tb::Graph g(cfg);
      for (int row = 1; row < cfg.height; ++row) {
        for (int col = 0; col < width; ++col) {
          const auto deps = g.deps(row, col);
          for (int dep : deps) {
            assert(dep >= 0);
            assert(dep < width);
          }
        }
      }
    }
  }
}
