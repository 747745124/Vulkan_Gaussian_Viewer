#include "object3d.h"

glm::vec3 Object3D::getFront() const {
	return rotation * defaultFront;
}

glm::vec3 Object3D::getUp() const{
	
	return rotation * defaultUp;
}

glm::vec3 Object3D::getRight() const{

	return rotation * defaultRight;
}

glm::mat4 Object3D::getModelMatrix() const {
	return glm::translate(glm::mat4(1.0f), position) *
		glm::mat4_cast(rotation) *
		glm::scale(glm::mat4(1.0f), scale);
}