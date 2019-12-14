class random_gen {
    mt19937_64 rand_;

  public:
    random_gen(mt19937_64::result_type seed) : rand_(seed) {}
    template <class I>
    auto get(I from = numeric_limits<I>::min(),
             I to = numeric_limits<I>::max()) {
        auto dist = uniform_int_distribution<I>(from, to);
        return dist(rand_);
    }
    auto get_real(double from = numeric_limits<double>::min(),
                  double to = numeric_limits<double>::max()) {
        auto dist = uniform_real_distribution<double>(from, to);
        return dist(rand_);
    }
    auto operator()() { return rand_(); }
    static constexpr auto min() { return decltype(rand_)::min(); }
    static constexpr auto max() { return decltype(rand_)::max(); }
};
