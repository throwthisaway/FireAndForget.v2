#include "pch.h"
#include "Input.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace {
const float pixelToTrRatio = .01f;
}
void Input::Start(float x, float y) {
	x_ = x; y_ = y;
}
void Input::Rotate(float x, float y, Transform& transform) {
	float dh = y - y_, dp = x - x_, db = 0.f;
	transform.rot.x += dh; transform.rot.y += dp; transform.rot.z += db;
	x_ = x; y_ = y;
}

void Input::TranslateXZ(float x, float y, Transform& transform) {
	float dx = x - x_, dy = 0.f, dz = y - y_;
	transform.pos.x += dx * pixelToTrRatio; transform.pos.y += dy * pixelToTrRatio; transform. pos.z += dz * pixelToTrRatio;
	x_ = x; y_ = y;
}
void Input::TranslateY(float y, Transform& transform) {
	float dx = 0.f, dy = y_ - y, dz = 0.f;
	transform.pos.x += dx * pixelToTrRatio; transform.pos.y += dy * pixelToTrRatio; transform.pos.z += dz * pixelToTrRatio;
	y_ = y;
}
