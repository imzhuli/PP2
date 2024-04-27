#pragma once
#include <core/core_min.hpp>
#include <core/ini.hpp>
#include <network/net_address.hpp>

class xConfig final : ::xel::xNonCopyable {

public:
	xConfig(const char * filename)
		: Reader(filename) {
	}
	X_INLINE operator bool() const {
		return Reader;
	}

	void Require(std::string & Dst, const char * Key);
	void Require(xel::xNetAddress & Dst, const char * Key);
	void Require(int64_t & Dst, const char * Key);
	template <typename T>
	std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<int64_t, T>> Require(T & Dst, const char * Key) {
		int64_t Temp;
		Require(Temp, Key);
		Dst = (T)Temp;
	}

	void Optional(std::string & Dst, const char * Key, const char * DefaultValue = "");
	void Optional(int64_t & Dst, const char * Key, int64_t DefaultValue = 0);
	void Optional(bool & Dst, const char * Key, bool DefaultValue = false);
	template <typename T, typename U>
	std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<int64_t, T> && !std::is_same_v<bool, T>> Optional(
		T & Dst, const char * Key, U DefaultValue = {}
	) {
		int64_t Temp;
		Optional(Temp, Key, DefaultValue);
		Dst = (T)Temp;
	}

private:
	xel::xIniReader Reader;
};
