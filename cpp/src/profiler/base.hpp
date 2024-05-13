#pragma once
#include "../common/base.hpp"

struct xProfile {
	size64_t Total      = 0;
	size64_t DurationMS = 0;
};

class xProfiler : xNonCopyable {
public:
	xProfiler() = default;
	xProfiler(const std::string & Name)
		: Name(Name) {
	}

	const std::string & GetName() const {
		return Name;
	}

	void Hit(size_t Step = 1) {
		Counter += Step;
	}

	xProfile Dump() {
		uint64_t Last     = Ticker();
		uint64_t Duration = Ticker.Update() - Last;
		return xProfile{
			.Total      = Steal(Counter),
			.DurationMS = Duration,
		};
	}

private:
	std::string Name;
	xTicker     Ticker  = {};
	size64_t    Counter = 0;
};

struct xConnectionProfile {
	uint64_t Start;
	uint64_t ConnectionDelay;
	uint64_t ConnectionDuration;
	size64_t UploadSize;
	size64_t DownloadSize;

	std::string ToString() const;
};

struct xConnectionProfiler : xNonCopyable {
public:
	xConnectionProfiler()  = default;
	~xConnectionProfiler() = default;

	void Reset() {
		xel::Reset(StartConnectionTimestamp);
		xel::Reset(EstablishConnectionTimestamp);
		xel::Reset(CloseConnectionTimestamp);
		xel::Reset(UploadSize);
		xel::Reset(DownloadSize);
	}

	xConnectionProfile Dump() {
		return xConnectionProfile{
			.Start              = StartConnectionTimestamp,
			.ConnectionDelay    = EstablishConnectionTimestamp - StartConnectionTimestamp,
			.ConnectionDuration = CloseConnectionTimestamp - StartConnectionTimestamp,
			.UploadSize         = UploadSize,
			.DownloadSize       = DownloadSize,
		};
	}

	void MarkStartConnection() {
		StartConnectionTimestamp = GetTimestampMS();
	}
	void MarkEstablished() {
		EstablishConnectionTimestamp = GetTimestampMS();
	}
	void MarkCloseConnection() {
		CloseConnectionTimestamp = GetTimestampMS();
	}
	size_t MarkUpload(size64_t S) {
		UploadSize += S;
		return S;
	}
	size_t MarkDownload(size64_t S) {
		DownloadSize += S;
		return S;
	}

private:
	uint64_t StartConnectionTimestamp     = 0;
	uint64_t EstablishConnectionTimestamp = 0;
	uint64_t CloseConnectionTimestamp     = 0;
	size64_t UploadSize                   = 0;
	size64_t DownloadSize                 = 0;
};
