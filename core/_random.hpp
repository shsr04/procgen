#include <random>
class random_gen {
  std::mt19937_64 rand_;

public:
  random_gen() : rand_(random_device()()) {}
  template<class I> auto get(I from=numeric_limits<I>::min(), I to=numeric_limits<I>::max()) {
    auto dist = uniform_int_distribution<I>(from, to);
    return dist(rand_);
  }
  auto get_real(double from=numeric_limits<double>::min(), size_t to=numeric_limits<double>::max()) {
    auto dist = uniform_real_distribution<double>(from, to);
    return dist(rand_);
  }
};
