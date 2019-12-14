#pragma once
#include <array>
#include <bitset>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <rangeless/include/fn.hpp>
#include <set>
#include <string>
#include <vector>

#ifdef USE_GMP
#include <gmpxx.h>
using big_int = mpz_class;
#else
using big_int = ssize_t;
#endif

using namespace std;
namespace f = rangeless::fn;
using f::operators::operator%, f::operators::operator%=;

/// Converts int literal to size_t
size_t operator""_s(unsigned long long p) { return static_cast<size_t>(p); }

#include <_graph.hpp>
#include <_iota.hpp>
#include <_random.hpp>
