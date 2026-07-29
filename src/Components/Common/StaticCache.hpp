#pragma once

#include "Components/Common/Cache.hpp"

template <typename T>
class StaticCache
{
public:
	StaticCache() = delete;
	~StaticCache() = delete;

	static void store(const uint64_t id, std::unique_ptr<T> value)
	{
		g_cache.store(id, std::move(value));
	}

	static void store(const uint64_t id, std::shared_ptr<T> value)
	{
		g_cache.store(id, std::move(value));
	}

	[[nodiscard]]
	static std::shared_ptr<T> find(const uint64_t id)
	{
		return g_cache.find(id);
	}

	[[nodiscard]]
	static bool contains(const uint64_t id)
	{
		return g_cache.contains(id);
	}

	static void erase(const uint64_t id)
	{
		g_cache.erase(id);
	}

	static void bulkRemove(const std::vector<uint64_t>& ids)
	{
		g_cache.bulkRemove(ids);
	}

	[[nodiscard]]
	static typename Cache<T>::Snapshot snapshot()
	{
		return g_cache.snapshot();
	}

private:
	inline static Cache<T> g_cache;
};
