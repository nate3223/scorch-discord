#include "Document.hpp"

template <typename T>
template <typename... Args>
std::vector<T> Document<T>::ContructVector(const bsoncxx::array::view& arr, Args&&... args)
{
	std::vector<T> items(arr.length);
	for (const auto& element : arr)
	{
		if (element.type() == type::k_document)
			items.emplace_back(element.get_document(), std::forward<Args>(args));
	}
	return items;
}

template <typename T>
template <typename... Args>
std::vector<std::unique_ptr<T>> Document<T>::ContructUniqueVector(const bsoncxx::array::view& arr, Args&&... args)
{
	std::vector<std::unique_ptr<T>> items(arr.length);
	for (const auto& element : arr)
	{
		if (element.type() == type::k_document)
			items.push_back(std::make_unique<T>(element.get_document(), std::forward<Args>(args)));
	}
	return items;
}

template <typename T>
array Document<T>::ConstructArray(const std::vector<T>& vec)
{
	array array;
	for (const auto& item : vec)
		array.append(item.getValue());
	return array;
}

template <typename T>
array Document<T>::ConstructArray(const std::vector<std::unique_ptr<T>>& vec)
{
	array array;
	for (const auto& item : vec)
		array.append(item->getValue());
	return array;
}