#include "./mysql_core_bind.hpp"

#include <mysql/mysql.h>

static_assert(sizeof(MYSQL_TIME) < xMySqlTimestamp::RESERVED_MYSQL_TIME_SIZE);

namespace __mysql_detail__ {

	int GetLongLongType() {
		return (int)MYSQL_TYPE_LONGLONG;
	}

	int GetFloatType() {
		return (int)MYSQL_TYPE_FLOAT;
	}

	int GetDoubleType() {
		return (int)MYSQL_TYPE_DOUBLE;
	}

	int GetTimestampType() {
		return (int)MYSQL_TYPE_TIMESTAMP;
	}

	int GetStringType() {
		return (int)MYSQL_TYPE_STRING;
	}

	int GetBinaryType() {
		return (int)MYSQL_TYPE_BLOB;
	}

}  // namespace __mysql_detail__

xMySqlTimestamp::xMySqlTimestamp() {  // no input or output
	Dummy.CreateValueAs<MYSQL_TIME>();
	this->Timestamp = 0;
}

xMySqlTimestamp::xMySqlTimestamp(int64_t Timestamp) {  // as input
	Dummy.CreateValueAs<MYSQL_TIME>();
	this->Timestamp = Timestamp;
	ParseTimestampToDummyObject();
}

xMySqlTimestamp::~xMySqlTimestamp() {
	Dummy.DestroyAs<MYSQL_TIME>();
}

void * xMySqlTimestamp::GetDummyObjectAddress() {
	return &Dummy.As<MYSQL_TIME>();
}

size_t xMySqlTimestamp::GetDummyObjectSize() {
	return sizeof(MYSQL_TIME);
}

void xMySqlTimestamp::ParseTimestampToDummyObject() {
	auto & T  = Dummy.As<MYSQL_TIME>();
	auto   TS = (time_t)Timestamp;
	auto   TM = tm();

	gmtime_r(&TS, &TM);
	T.year      = TM.tm_year + 1900;
	T.month     = TM.tm_mon + 1;
	T.day       = TM.tm_mday;
	T.hour      = TM.tm_hour;
	T.minute    = TM.tm_min;
	T.second    = TM.tm_sec;
	T.time_type = MYSQL_TIMESTAMP_DATETIME;

	X_DEBUG_PRINTF("Timestamp: type=%i y=%i, m=%i, d=%i, hour=%i, min=%i, sec=%i", (int)T.time_type, T.year, T.month, T.day, T.hour, T.minute, T.second);
}

void xMySqlTimestamp::ExtractTimestampFromDummyObject() {
	auto & T = Dummy.As<MYSQL_TIME>();
	X_DEBUG_PRINTF("Timestamp: type=%i y=%i, m=%i, d=%i, hour=%i, min=%i, sec=%i", (int)T.time_type, T.year, T.month, T.day, T.hour, T.minute, T.second);

	auto TM    = tm{};
	TM.tm_year = T.year - 1900;
	TM.tm_mon  = T.month - 1;
	TM.tm_mday = T.day;
	TM.tm_hour = T.hour;
	TM.tm_min  = T.minute;
	TM.tm_sec  = T.second;
	Timestamp  = (int64_t)MakeSigned(timegm(&TM));
}

void * AllocBinds(size_t Count) {
	return calloc(Count, sizeof(MYSQL_BIND));
}

void * NextBind(void * _) {
	auto Bind = (MYSQL_BIND *)_;
	return Bind + 1;
}

void * NextBindN(void * _, size_t N) {
	auto Bind = (MYSQL_BIND *)_;
	return Bind + N;
}

void FreeBinds(void * BindPtr) {
	free(BindPtr);
}

void xOutBind::SetupIntegral(void * BindPoint, void * OutputPtr, xFetcher Fetcher) {
	this->TargetPtr   = OutputPtr;
	this->Fetcher     = Fetcher;
	auto BP           = (MYSQL_BIND *)BindPoint;
	BP->buffer_type   = MYSQL_TYPE_LONGLONG;
	BP->buffer        = &this->Receiver.LL;
	BP->buffer_length = sizeof(this->Receiver.LL);
}

void xOutBind::Setup(void * BindPoint, size_t Column, xMySqlTimestamp & Output) {
	this->TargetPtr   = &Output;
	this->Fetcher     = &FetchTimestamp;
	auto BP           = (MYSQL_BIND *)BindPoint;
	BP->buffer_type   = MYSQL_TYPE_TIMESTAMP;
	BP->buffer        = Output.GetDummyObjectAddress();
	BP->buffer_length = Output.GetDummyObjectSize();
	// clear old value:
	Reset(Output.Timestamp);
}

void xOutBind::Setup(void * BindPoint, size_t Column, std::string & Output) {
	TargetPtr          = &Output;
	Fetcher            = &FetchString;
	Receiver.BindPoint = BindPoint;
	Receiver.Column    = Column;
	Receiver.Length    = 0;
	auto BP            = (MYSQL_BIND *)BindPoint;
	BP->buffer_type    = MYSQL_TYPE_STRING;
	BP->buffer         = nullptr;
	BP->buffer_length  = 0;
	BP->length         = &Receiver.Length;
	// clear old value:
	Output.clear();
}

bool xOutBind::FetchTimestamp(xOutBind * B, void * Stmt) {
	auto PT = (xMySqlTimestamp *)B->TargetPtr;
	PT->ExtractTimestampFromDummyObject();
	X_DEBUG_PRINTF("TIMESTAMP=%" PRIi64 "", PT->Timestamp);
	return true;
}

bool xOutBind::FetchString(xOutBind * B, void * Stmt) {
	auto PS = (std::string *)B->TargetPtr;
	auto BP = (MYSQL_BIND *)B->Receiver.BindPoint;
	if (!B->Receiver.Length) {
		return true;
	}
	PS->resize(B->Receiver.Length);
	BP->buffer        = PS->data();
	BP->buffer_length = PS->length();
	return !mysql_stmt_fetch_column((MYSQL_STMT *)Stmt, (MYSQL_BIND *)B->Receiver.BindPoint, B->Receiver.Column, 0);
}
