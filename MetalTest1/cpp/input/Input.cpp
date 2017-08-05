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
