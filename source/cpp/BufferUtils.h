#pragma once
template<uint16_t Alignment, typename T> constexpr T AlignTo(T val) { return (val + Alignment - 1) & ~(Alignment - 1); }

