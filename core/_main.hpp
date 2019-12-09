#pragma once
#include <array>
#include <bitset>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

#ifdef USE_GMP
#include <gmpxx.h>
using big_int = mpz_class;
#else
using big_int = ssize_t;
#endif

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

using namespace std;
namespace r = ranges::cpp20;
namespace v = r::views;

/// Converts int literal to size_t
size_t operator""_s(unsigned long long p) { return static_cast<size_t>(p); }

#include <_graph.hpp>
#include <_screen.hpp>
#include <_random.hpp>
