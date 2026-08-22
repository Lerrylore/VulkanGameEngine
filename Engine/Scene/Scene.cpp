#include "Scene.h"

#include "GameObject.h"

Scene::~Scene() = default;

GameObject& Scene::createGameObject()
{
	auto gameObject = std::make_unique<GameObject>();
	auto& result = *gameObject;
	gameObjects_.push_back(std::move(gameObject));
	return result;
}
