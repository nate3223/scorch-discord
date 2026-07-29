#include "Components/Common/Cache.hpp"
#include "Components/Common/StaticCache.hpp"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

struct Value
{
	uint64_t id;
};

}

int main()
{
	Cache<Value> cache;
	cache.store(1, std::make_unique<Value>(1));

	auto retained = cache.find(1);
	cache.erase(1);
	assert(! cache.find(1));
	assert(retained && retained->id == 1);

	cache.store(2, std::make_unique<Value>(2));
	auto snapshot = cache.snapshot();
	cache.bulkRemove({ 2 });
	assert(! cache.find(2));
	assert(snapshot.size() == 1);
	assert(snapshot.front().second->id == 2);

	StaticCache<Value>::store(4, std::make_unique<Value>(4));
	auto staticRetained = StaticCache<Value>::find(4);
	StaticCache<Value>::erase(4);
	assert(staticRetained && staticRetained->id == 4);

	std::atomic_bool done = false;
	std::thread reader([&] {
		while (! done.load(std::memory_order_relaxed))
		{
			if (auto value = cache.find(3))
				assert(value->id == 3);
		}
	});

	for (int i = 0; i < 10'000; ++i)
	{
		cache.store(3, std::make_shared<Value>(3));
		cache.erase(3);
	}

	done.store(true, std::memory_order_relaxed);
	reader.join();
}
