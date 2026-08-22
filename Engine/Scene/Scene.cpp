#include "Scene.h"

#include "GameObject.h"

#include <stdexcept>

Scene::~Scene()
{
	Destroy();
}

GameObject& Scene::CreateGameObject()
{
	if (bDestroyed)
	{
		throw std::logic_error("Cannot create a GameObject in a destroyed Scene");
	}

	auto gameObject = std::make_unique<GameObject>();
	auto& result = *gameObject;
	GameObjects.push_back(std::move(gameObject));

	if (bInitialized)
	{
		try
		{
			result.Initialize();
		}
		catch (...)
		{
			GameObjects.pop_back();
			throw;
		}
	}
	return result;
}

bool Scene::IsInitialized() const noexcept
{
	return bInitialized;
}

void Scene::Initialize()
{
	if (bDestroyed)
	{
		throw std::logic_error("Cannot initialize a destroyed Scene");
	}
	if (bInitialized)
	{
		return;
	}

	try
	{
		for (size_t index = 0; index < GameObjects.size(); ++index)
		{
			GameObjects[index]->Initialize();
		}
		bInitialized = true;
	}
	catch (...)
	{
		Destroy();
		throw;
	}
}

void Scene::Update(float deltaTime)
{
	if (!bInitialized || bDestroyed)
	{
		return;
	}

	const size_t gameObjectCount = GameObjects.size();
	for (size_t index = 0; index < gameObjectCount; ++index)
	{
		GameObjects[index]->Update(deltaTime);
	}
}

void Scene::Destroy() noexcept
{
	if (bDestroyed)
	{
		return;
	}

	bDestroyed = true;
	bInitialized = false;
	for (auto gameObject = GameObjects.rbegin(); gameObject != GameObjects.rend(); ++gameObject)
	{
		(*gameObject)->Destroy();
	}
}
