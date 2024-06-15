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