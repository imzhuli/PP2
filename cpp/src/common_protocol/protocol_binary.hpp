#pragma once
#include "../common/base.hpp"

class xBinaryMessage {

public:
	static_assert(std::is_trivially_destructible_v<xStreamReader>);
	static_assert(std::is_trivially_destructible_v<xStreamWriter>);

	size_t Serialize(void * Dst, size_t Size) {
		auto SSize = static_cast<ssize_t>(Size);
		assert(SSize < std::numeric_limits<ssize_t>::max());

		_Writer.Reset(Dst);
		_RemainWriteSize = SSize;
		InternalSerialize();
		if (Steal(_RemainWriteSize) < 0) {
			return 0;
		}
		return _Writer.Offset();
	}
	size_t Deserialize(const void * Src, size_t Size) {
		auto SSize = static_cast<ssize_t>(Size);
		assert(SSize < std::numeric_limits<ssize_t>::max());

		_Reader.Reset(Src);
		_RemainReadSize = SSize;
		InternalDeserialize();
		Reset(_RemainReadSize);
		return _Reader.Offset();
	}

private:
	virtual void InternalSerialize() {
		_RemainWriteSize = -1;
	}
	virtual void InternalDeserialize() {
		_RemainReadSize = -1;
	}

protected:
	X_INLINE void W1(uint8_t V) {
		auto RequiredSize = (ssize_t)sizeof(V);
		if (_RemainWriteSize < RequiredSize) {
			_RemainWriteSize = -1;
			return;
		}
		_RemainWriteSize -= RequiredSize;
		_Writer.W1L(V);
	}
	X_INLINE void W2(uint16_t V) {
		auto RequiredSize = (ssize_t)sizeof(V);
		if (_RemainWriteSize < RequiredSize) {
			_RemainWriteSize = -1;
			return;
		}
		_RemainWriteSize -= RequiredSize;
		_Writer.W2L(V);
	}
	X_INLINE void W4(uint32_t V) {
		auto RequiredSize = (ssize_t)sizeof(V);
		if (_RemainWriteSize < RequiredSize) {
			_RemainWriteSize = -1;
			return;
		}
		_RemainWriteSize -= RequiredSize;
		_Writer.W4L(V);
	}
	X_INLINE void W8(uint64_t V) {
		auto RequiredSize = (ssize_t)sizeof(V);
		if (_RemainWriteSize < RequiredSize) {
			_RemainWriteSize = -1;
			return;
		}
		_RemainWriteSize -= RequiredSize;
		_Writer.W8L(V);
	}
	X_INLINE void WB(const void * Block, size_t Size) {
		W4((ssize32_t)Size);
		if (_RemainWriteSize < 0) {
			return;
		}
		WriteRawBlock(Block, Size);
	}

	//

	X_INLINE uint8_t R1() {
		auto RequiredSize = (ssize_t)sizeof(uint8_t);
		if (_RemainReadSize < RequiredSize) {
			_RemainReadSize = -1;
			return 0;
		}
		_RemainReadSize -= RequiredSize;
		return _Reader.R1L();
	}
	X_INLINE uint16_t R2() {
		auto RequiredSize = (ssize_t)sizeof(uint16_t);
		if (_RemainReadSize < RequiredSize) {
			_RemainReadSize = -1;
			return 0;
		}
		_RemainReadSize -= RequiredSize;
		return _Reader.R2L();
	}
	X_INLINE uint32_t R4() {
		auto RequiredSize = (ssize_t)sizeof(uint32_t);
		if (_RemainReadSize < RequiredSize) {
			_RemainReadSize = -1;
			return 0;
		}
		_RemainReadSize -= RequiredSize;
		return _Reader.R4L();
	}
	X_INLINE uint64_t R8() {
		auto RequiredSize = (ssize_t)sizeof(uint64_t);
		if (_RemainReadSize < RequiredSize) {
			_RemainReadSize = -1;
			return 0;
		}
		_RemainReadSize -= RequiredSize;
		return _Reader.R8L();
	}

	X_INLINE std::string RB() {
		auto Size = R4();
		if (_RemainReadSize < 0) {
			return {};
		}
		auto R = std::string();
		R.resize(Size);
		ReadRawBlock(R.data(), Size);
		return R;
	}

private:
	X_INLINE void WriteRawBlock(const void * Block, size_t Size) {
		auto RequiredSize = static_cast<ssize_t>(Size);
		assert(RequiredSize < std::numeric_limits<ssize32_t>::max());
		if (_RemainWriteSize < RequiredSize) {
			_RemainWriteSize = -1;
			return;
		}
		_RemainWriteSize -= RequiredSize;
		_Writer.W(Block, Size);
	}

	X_INLINE void ReadRawBlock(void * Block, size_t Size) {
		auto RequiredSize = static_cast<ssize_t>(Size);
		assert(RequiredSize < std::numeric_limits<ssize32_t>::max());
		if (_RemainReadSize < RequiredSize) {
			_RemainReadSize = -1;
			return;
		}
		_RemainReadSize -= RequiredSize;
		_Reader.R(Block, Size);
	}

private:
	xStreamWriter _Writer;
	ssize_t       _RemainWriteSize = 0;

	xStreamReader _Reader;
	ssize_t       _RemainReadSize = 0;
};