#include "Scene.h"

#include "GameObject.h"

#include <stdexcept>

Scene::~Scene()
{
	destroy();
}

GameObject& Scene::createGameObject()
{
	if (destroyed_)
	{
		throw std::logic_error("Cannot create a GameObject in a destroyed Scene");
	}

	auto gameObject = std::make_unique<GameObject>();
	auto& result = *gameObject;
	gameObjects_.push_back(std::move(gameObject));

	if (initialized_)
	{
		try
		{
			result.initialize();
		}
		catch (...)
		{
			gameObjects_.pop_back();
			throw;
		}
	}
	return result;
}

bool Scene::isInitialized() const noexcept
{
	return initialized_;
}

void Scene::initialize()
{
	if (destroyed_)
	{
		throw std::logic_error("Cannot initialize a destroyed Scene");
	}
	if (initialized_)
	{
		return;
	}

	try
	{
		for (size_t index = 0; index < gameObjects_.size(); ++index)
		{
			gameObjects_[index]->initialize();
		}
		initialized_ = true;
	}
	catch (...)
	{
		destroy();
		throw;
	}
}

void Scene::update(float deltaTime)
{
	if (!initialized_ || destroyed_)
	{
		return;
	}

	const size_t gameObjectCount = gameObjects_.size();
	for (size_t index = 0; index < gameObjectCount; ++index)
	{
		gameObjects_[index]->update(deltaTime);
	}
}

void Scene::destroy() noexcept
{
	if (destroyed_)
	{
		return;
	}

	destroyed_ = true;
	initialized_ = false;
	for (auto gameObject = gameObjects_.rbegin(); gameObject != gameObjects_.rend(); ++gameObject)
	{
		(*gameObject)->destroy();
	}
}
