#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

class ServiceLocator final
{
  public:
	ServiceLocator() = default;
	~ServiceLocator() = default;

	ServiceLocator(const ServiceLocator&) = delete;
	ServiceLocator& operator=(const ServiceLocator&) = delete;
	ServiceLocator(ServiceLocator&&) = delete;
	ServiceLocator& operator=(ServiceLocator&&) = delete;

	template <typename TService>
	void Register(TService& service)
	{
		Register(typeid(TService), std::addressof(service));
	}

	template <typename TService>
	[[nodiscard]] TService& Get()
	{
		return *static_cast<TService*>(Get(typeid(TService)));
	}

	template <typename TService>
	[[nodiscard]] const TService& Get() const
	{
		return *static_cast<const TService*>(Get(typeid(TService)));
	}

	template <typename TService>
	[[nodiscard]] bool Has() const noexcept
	{
		return Has(typeid(TService));
	}

	template <typename TService>
	bool Unregister() noexcept
	{
		return Unregister(typeid(TService));
	}

  private:
	void Register(const std::type_info& serviceType, void* service);
	[[nodiscard]] void* Get(const std::type_info& serviceType);
	[[nodiscard]] const void* Get(const std::type_info& serviceType) const;
	[[nodiscard]] bool Has(const std::type_info& serviceType) const noexcept;
	bool Unregister(const std::type_info& serviceType) noexcept;

	std::unordered_map<std::type_index, void*> Services;
};
