#pragma once

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>

using bsoncxx::type;
using bsoncxx::builder::basic::array;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;


template <typename T>
class Document
{
public:
	virtual ~Document() {}

	virtual bsoncxx::document::value getValue() const = 0;

	static std::vector<T> ContructVector(const bsoncxx::array::view& arr)
	{
		std::vector<T> items;
		for (const auto& element : arr)
		{
			if (element.type() == type::k_document)
				items.emplace_back(element.get_document());
		}
		return items;
	}

	static array ConstructArray(const std::vector<T>& vec)
	{
		array array;
		for (const auto& item : vec)
			array.append(item.getValue());
		return array;
	}
};