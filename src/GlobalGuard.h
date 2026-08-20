#pragma once
#include <mutex>

template <typename T>
struct GlobalGuard
{
	template <typename F>
	auto with_lock(F&& f)
	{
		std::lock_guard lock(mutex);
		return f(value);
	}
	private:
	T value;
	std::mutex mutex;
};


