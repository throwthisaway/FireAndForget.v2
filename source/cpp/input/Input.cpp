#include "pch.h"
#include "Input.hpp"

namespace {
const float pixelToTrRatio = .01f;
const float pixelToRadRatio = .01f;
}
void Input::Start(float x, float y) {
	x_ = x; y_ = y;
	dpos = drot = {};
}
float3 Input::Rotate(float x, float y) {
	float dh = y - y_, dp = x - x_, db = 0.f;
	x_ = x; y_ = y;
	dpos = {};
	return drot = float3{ dh, dp, db } * pixelToRadRatio;
}

float3 Input::TranslateXZ(float x, float y) {
	float dx = x_ - x , dy = 0.f, dz = y - y_;
	x_ = x; y_ = y;
	drot = {};
	return dpos = float3{ dx, dy, dz } * pixelToTrRatio;
	
}
float3 Input::TranslateY(float y) {
	float dx = 0.f, dy = y_ - y, dz = 0.f;
	y_ = y;
		drot = {};
	return dpos = float3{ dx, dy, dz } * pixelToTrRatio;
}
