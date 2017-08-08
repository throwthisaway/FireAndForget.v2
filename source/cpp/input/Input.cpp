#include "Input.hpp"
#include <glm/gtc/matrix_transform.hpp>

void Input::Start(float x, float y) {
	x_ = x; y_ = y;
}
void Input::Rotate(float x, float y) {
	if (camera_)
		camera_->Rotate(y_ - y, x - x_);
	x_ = x; y_ = y;
}

void Input::TranslateXZ(float x, float y) {
	if (camera_)
		camera_->Translate(x - x_, 0.f, y_ - y);
	x_ = x; y_ = y;
}
void Input::TranslateY(float y) {
	if (camera_)
		camera_->Translate(0.f, y - y_, 0.f);
	y_ = y;
}

