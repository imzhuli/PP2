#include "./mysql_core_stmt.hpp"

#include <mysql/mysql.h>

#ifndef ER_CONNECTION_KILLED
#define ER_CONNECTION_KILLED 1927
#endif
static inline bool IsStmtServerLostError(MYSQL_STMT * Stmt) {
	auto ErrorCode = mysql_stmt_errno(Stmt);
	if (ErrorCode == CR_SERVER_GONE_ERROR || ErrorCode == CR_SERVER_LOST || ErrorCode == ER_CONNECTION_KILLED) {
		[[maybe_unused]] auto TS = std::ctime(X2P(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));
		X_DEBUG_PRINTF("Checking server-lost error: @%s  code=%u: %s", TS, ErrorCode, mysql_stmt_error(Stmt));
		return true;
	}
	if (ErrorCode) {
		auto ErrorCodeStr = mysql_stmt_error(Stmt);
		if (ErrorCodeStr) {
			X_DEBUG_PRINTF("Checking non-server-lost error: %u: %s", ErrorCode, mysql_stmt_error(Stmt));
		}
	}
	return false;
}

bool xMySqlStmt::Init(xMySqlConn & MySqlConn, const std::string & Query) {
	this->Conn  = &MySqlConn;
	this->Query = Query;

	Reset(Native);
	Reset(LastInsertId);
	Reset(LastResultRows);
	Reset(LastAffectedRows);
	return true;
}

void xMySqlStmt::Clean() {
	TestAndClose();
	Renew(Query);
	Reset(Conn);
}

void xMySqlStmt::Open() {
	assert(!Native);
	auto MySqlConn = (MYSQL *)Conn->GetNative();
	if (!MySqlConn) {
		return;
	}
	auto Stmt = mysql_stmt_init((MYSQL *)Conn->GetNative());
	if (!Stmt) {
		return;
	}
	if (mysql_stmt_prepare(Stmt, Query.data(), Query.length())) {
		if (IsStmtServerLostError(Stmt)) {
			Conn->SetDisconnected();
		}
		mysql_stmt_close(Stmt);
		return;
	}
	Native = Stmt;
}

void xMySqlStmt::Close() {
	assert(Native);
	if (mysql_stmt_close((MYSQL_STMT *)Steal(Native))) {
		Conn->SetDisconnected();
	}
}

void xMySqlStmt::TestAndClose() {
	if (!Native) {
		return;
	}
	Close();
}

bool xMySqlStmt::CheckReonnection() {
	auto NewConnVersion = Conn->GetVersion();
	if (!NewConnVersion.IsEqualTo(ConnVersion)) {
		TestAndClose();
		if (!NewConnVersion.IsValid()) {
			return false;
		}
		Open();
		if (!Native) {
			return false;
		}
		ConnVersion = NewConnVersion;
	}
	return true;
}

bool xMySqlStmt::ProcessServerLostError() {
	auto Stmt = (MYSQL_STMT *)Native;
	assert(Stmt);
	if (IsStmtServerLostError(Stmt)) {
		Close();
		Conn->SetDisconnected();
		return true;
	}
	return false;
}

// binding and executing:
void xMySqlStmt::InternalSetupInBind(void * BindPtr, int Type, void * BufferPtr, unsigned long Length) {
	auto Bind           = (MYSQL_BIND *)BindPtr;
	Bind->buffer_type   = (enum_field_types)Type;
	Bind->buffer        = BufferPtr;
	Bind->buffer_length = Length;
}

bool xMySqlStmt::InternalBindAndExecute(xOnRowRecordWithCtx Callback, xVariable UserCtx, void * InputBind, void * OutputBind, xOutBind * OutputBindDelegate) {
	assert(Native);  // connection checked before entering this procedure
	auto ParamBind  = (MYSQL_BIND *)InputBind;
	auto ResultBind = (MYSQL_BIND *)OutputBind;
	auto Stmt       = (MYSQL_STMT *)Native;

	if (mysql_stmt_bind_param(Stmt, ParamBind) || (OutputBindDelegate && mysql_stmt_bind_result(Stmt, ResultBind)) || mysql_stmt_execute(Stmt)) {
		X_DEBUG_PRINTF("Faild to bind param, or to execute statement");
		ProcessServerLostError();
		return false;
	}
	if (mysql_stmt_store_result(Stmt)) {
		X_DEBUG_PRINTF("Failed to store result");
		ProcessServerLostError();
		return false;
	}
	auto ResG = xScopeGuard([&] { mysql_stmt_free_result(Stmt); });  // dismiss on server lost error ?

	LastInsertId     = (uint64_t)mysql_stmt_insert_id(Stmt);
	LastResultRows   = (size_t)mysql_stmt_num_rows(Stmt);
	LastAffectedRows = (size_t)mysql_stmt_affected_rows(Stmt);

	if (LastResultRows) {
		size_t Fields = (size_t)mysql_stmt_field_count(Stmt);
		do {
			auto FR = mysql_stmt_fetch(Stmt);
			if (FR && FR != MYSQL_DATA_TRUNCATED) {
				break;
			}
			for (size_t i = 0; i < Fields; ++i) {
				auto OutBindPtr = &OutputBindDelegate[i];
				if (!OutBindPtr->GetFetcher()(OutBindPtr, Stmt)) {
					if (ProcessServerLostError()) {
						cerr << "CheckBug: (OutBindFetcher)this should never happen, since result is stored locally" << endl;
						ResG.Dismiss();
					}
					return false;
				}
			}
			Callback(UserCtx);
		} while (true);
		assert(!ProcessServerLostError());
	}
	return !ProcessServerLostError();
}
