#pragma once

#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

template <typename T>
class Cache
{
public:
	using Value = std::shared_ptr<T>;
	using Snapshot = std::vector<std::pair<uint64_t, Value>>;

	void store(const uint64_t id, std::unique_ptr<T> value)
	{
		store(id, Value(std::move(value)));
	}

	void store(const uint64_t id, Value value)
	{
		std::unique_lock lock(m_mutex);
		m_map.insert_or_assign(id, std::move(value));
	}

	[[nodiscard]]
	Value find(const uint64_t id) const
	{
		std::shared_lock lock(m_mutex);

		if (const auto it = m_map.find(id); it != m_map.end())
			return it->second;

		return nullptr;
	}

	[[nodiscard]]
	bool contains(const uint64_t id) const
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

	[[nodiscard]]
	Snapshot snapshot() const
	{
		std::shared_lock lock(m_mutex);

		Snapshot result;
		result.reserve(m_map.size());
		for (const auto& [id, value] : m_map)
			result.emplace_back(id, value);

		return result;
	}

private:
	mutable std::shared_mutex							m_mutex;
	boost::unordered_flat_map<uint64_t, Value>	m_map;
};
