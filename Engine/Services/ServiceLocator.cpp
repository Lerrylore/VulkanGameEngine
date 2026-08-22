#include "ServiceLocator.h"

#include <stdexcept>

void ServiceLocator::Register(const std::type_info& serviceType, void* service)
{
	const bool bInserted = Services.emplace(std::type_index(serviceType), service).second;
	if (!bInserted)
	{
		throw std::logic_error("A service of this type is already registered");
	}
}

void* ServiceLocator::Get(const std::type_info& serviceType)
{
	const auto service = Services.find(std::type_index(serviceType));
	if (service == Services.end())
	{
		throw std::logic_error("The requested service is not registered");
	}
	return service->second;
}

const void* ServiceLocator::Get(const std::type_info& serviceType) const
{
	const auto service = Services.find(std::type_index(serviceType));
	if (service == Services.end())
	{
		throw std::logic_error("The requested service is not registered");
	}
	return service->second;
}

bool ServiceLocator::Has(const std::type_info& serviceType) const noexcept
{
	return Services.contains(std::type_index(serviceType));
}

bool ServiceLocator::Unregister(const std::type_info& serviceType) noexcept
{
	return Services.erase(std::type_index(serviceType)) != 0;
}
