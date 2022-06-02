#include "thread_name_builder.h"
#include <cxxabi.h>
#include <cstring>

namespace holo
{
namespace winterfell
{

ThreadNameBuilder::ThreadNameBuilder(const char* class_name, const char* func_name, const char* explain)
{
	thread_name_.reserve(thread_name_capacity);
	// priority to use explain
	if (!isStrEmpty(explain))
	{
		thread_name_.append(explain);
		tryTruncate(thread_name_max_len);
		return;
	}

	// namespace
	if (!isStrEmpty(class_name) && isNameLengthValid(std::strlen(class_name) + 2))
	{
		thread_name_.append(demangle(class_name));
		thread_name_.append("::");
	}

	if (!isNameLengthValid(std::strlen(func_name)))
	{
		thread_name_.clear();
	}

	// func name
	thread_name_.append(func_name);

	// truncation for 16 char
	tryTruncate(thread_name_max_len);
}

const std::string& ThreadNameBuilder::Name() const
{
	return thread_name_;
}

bool ThreadNameBuilder::isStrEmpty(const char* str)
{
	return str[0] == 0;
}

bool ThreadNameBuilder::isNameLengthValid(int need_free_len)
{
	return thread_name_max_len - static_cast<int>(thread_name_.size()) >= need_free_len;
}

void ThreadNameBuilder::tryTruncate(int max_len)
{
	if (!isNameLengthValid())
	{
		thread_name_.assign(thread_name_.substr(0, max_len));
	}
}

const std::string ThreadNameBuilder::demangle(const char* name)
{
	int               status         = -4;
	char*             res            = abi::__cxa_demangle(name, NULL, NULL, &status);
	const char* const demangled_name = (status == 0) ? res : name;
	std::string       ret_val(demangled_name);
	free(res);
	return ret_val;
}

}  // namespace winterfell
}  // namespace holo
