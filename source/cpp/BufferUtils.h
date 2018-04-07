#pragma once
template<typename T, uint8_t Alignment> constexpr T AlignTo(T val) { return (val + Alignment - 1) & ~(Alignment - 1); }

