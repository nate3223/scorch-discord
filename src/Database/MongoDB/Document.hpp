#pragma once

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <memory>

using bsoncxx::type;
using bsoncxx::builder::basic::array;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;

namespace MongoDB
{
	constexpr char DATABASE_NAME[]	= "Scorch";
}

template <typename T>
class Document
{
public:
	virtual ~Document() {}

	virtual bsoncxx::document::value getValue() const = 0;

	template <typename... Args>
	static std::vector<T> ContructVector(const bsoncxx::array::view& arr, Args&&... arg);

	template <typename... Args>
	static std::vector<std::unique_ptr<T>> ContructUniqueVector(const bsoncxx::array::view& arr, Args&&... args);

	static array ConstructArray(const std::vector<T>& vec);

	static array ConstructArray(const std::vector<std::unique_ptr<T>>& vec);
};

template<typename T>
template<typename ...Args>
std::vector<T> Document<T>::ContructVector(const bsoncxx::array::view& arr, Args && ...args)
{
	std::vector<T> items(arr.length);
	for (const auto& element : arr)
	{
		if (element.type() == type::k_document)
			items.emplace_back(element.get_document(), std::forward<Args>(args)...);
	}
	return items;
}

template<typename T>
template<typename ...Args>
std::vector<std::unique_ptr<T>> Document<T>::ContructUniqueVector(const bsoncxx::array::view& arr, Args && ...args)
{
	std::vector<std::unique_ptr<T>> items(arr.length);
	for (const auto& element : arr)
	{
		if (element.type() == type::k_document)
			items.push_back(std::make_unique<T>(element.get_document(), std::forward<Args>(args)...));
	}
	return items;
}

template<typename T>
array Document<T>::ConstructArray(const std::vector<T>& vec)
{
	array array;
	for (const auto& item : vec)
		array.append(item.getValue());
	return array;
}

template<typename T>
array Document<T>::ConstructArray(const std::vector<std::unique_ptr<T>>& vec)
{
	array array;
	for (const auto& item : vec)
		array.append(item->getValue());
	return array;
}