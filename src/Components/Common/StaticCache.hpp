#include "Cache.hpp"

#define DEFINE_STATIC_CACHE(ClassName, CacheType)                       \
class ClassName                                                         \
{                                                                       \
public:                                                                 \
    ClassName() = delete;                                               \
    ~ClassName() = delete;                                              \
                                                                        \
    static void store(const uint64_t id, std::unique_ptr<CacheType> value) \
    {                                                                   \
        g_cache.store(id, std::move(value));                            \
    }                                                                   \
                                                                        \
    static CacheType* find(const uint64_t id)                           \
    {                                                                   \
        return g_cache.find(id);                                        \
    }                                                                   \
                                                                        \
    static bool contains(const uint64_t id)                             \
    {                                                                   \
        return g_cache.contains(id);                                    \
    }                                                                   \
                                                                        \
    static void erase(const uint64_t id)                                \
    {                                                                   \
        g_cache.erase(id);                                              \
    }                                                                   \
                                                                        \
    static void bulkRemove(const std::vector<uint64_t>& ids)            \
    {                                                                   \
        g_cache.bulkRemove(ids);                                        \
    }                                                                   \
                                                                        \
    static boost::unordered_flat_map<uint64_t, std::unique_ptr<CacheType>>::iterator begin() \
    {                                                                   \
        return g_cache.begin();                                         \
    }                                                                   \
                                                                        \
    static boost::unordered_flat_map<uint64_t, std::unique_ptr<CacheType>>::iterator end() \
    {                                                                   \
        return g_cache.end();                                           \
    }                                                                   \
                                                                        \
protected:                                                              \
    static Cache<CacheType> g_cache;                                    \
};