#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <random>
#include <rangeless/include/fn.hpp>
#include <set>
#include <string>
#include <thread>
#include <vector>

using nat = unsigned long long; // C++ guarantees >=64 bits for ULL
using sig = signed long long;

using namespace std;
namespace f = rangeless::fn;
using f::operators::operator%, f::operators::operator%=;

/// Converts int literal to size_t
size_t operator""_s(unsigned long long p) { return static_cast<size_t>(p); }

#include <_graph.hpp>
#include <_iota.hpp>
#include <_random.hpp>
