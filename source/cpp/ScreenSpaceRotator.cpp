#include "pch.h"
#include "ScreenSpaceRotator.h"
#include "Transform.h"
#include "MatrixUtils.h"


glm::mat4 ScreenSpaceRotator::Update(const Transform& transform) {
	glm::mat4 mat = RotationMatrix(glm::translate({}, transform.pos + transform.center), transform.rot.x, transform.rot.y, transform.rot.z);

	mat *= glm::mat4(rot);
	// copy topleft 3x3 matrix
	rot = glm::mat3(mat);
	mat = glm::translate(mat, -transform.center);
	return mat;
}