#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <shared_mutex>
#include <vector>

template <typename T>
class Cache
{
public:

	void store(const uint64_t id, std::unique_ptr<T> value)
	{
		std::unique_lock lock(m_mutex);
		m_map[id] = std::move(value);
	}

	T* find(const uint64_t id)
	{
		std::shared_lock lock(m_mutex);
		auto it = m_map.find(id);
		if (it != m_map.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	bool contains(const uint64_t id)
	{
		std::shared_lock lock(m_mutex);
		return m_map.contains(id);
	}

	void erase(const uint64_t id)
	{
		std::unique_lock lock(m_mutex);
		m_map.erase(id);
	}

	void bulkRemove(const std::vector<uint64_t>& ids)
	{
		std::unique_lock lock(m_mutex);
		for (const uint64_t id : ids)
			m_map.erase(id);
	}

	boost::unordered_flat_map<uint64_t, std::unique_ptr<T>>::iterator begin()
	{
		return m_map.begin();
	}

	boost::unordered_flat_map<uint64_t, std::unique_ptr<T>>::iterator end()
	{
		return m_map.end();
	}

private:
	std::shared_mutex	m_mutex;
	boost::unordered_flat_map<uint64_t, std::unique_ptr<T>>	m_map;
};