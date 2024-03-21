#pragma once
#include "./base.hpp"

#include <algorithm>

template <typename T>
class xDoubleBuffer : xNonCopyable {
	static_assert(!std::is_reference_v<T>);

public:
	bool Init() {
		Current = new T();
		Backup  = new T();
		return true;
	}

	void Clean() {
		delete Steal(Current);
		delete Steal(Backup);
	}

	const T * Get() {
		auto G = xSpinlockGuard(SwapLock);
		if (Steal(Updated)) {
			std::swap(Current, Backup);
		}
		return Current;
	}

	void Update(const T & New) {
		auto G  = xSpinlockGuard(SwapLock);
		*Backup = New;
		Updated = true;
	}

	void Update(T && New) {
		auto G  = xSpinlockGuard(SwapLock);
		*Backup = std::move(New);
		Updated = true;
	}

private:
	xSpinlock SwapLock;
	T *       Current = nullptr;
	T *       Backup  = nullptr;
	bool      Updated = false;
};
