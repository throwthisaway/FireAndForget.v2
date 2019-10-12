#pragma once
#include <chrono>

class Timer
{
	std::chrono::time_point<std::chrono::high_resolution_clock> _current, _prev, _start;
public:
	Timer();
	inline void Tick() {
		_prev = _current;
		_current = std::chrono::high_resolution_clock::now();
	}
	inline double FrameMs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(_current - _prev).count() / 1000000.; }
	inline double TotalMs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(_current - _start).count() / 1000000.; }
	static double NowMs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() / 1000000.; }
};

struct Time {
	double total, frame;
};
