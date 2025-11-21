#include "./mysql_core_transaction.hpp"

#include <mysql/mysql.h>

bool xMySqlTransaction::StartTransaction() {
	auto MySql = (MYSQL *)Conn->GetNative();
	assert(MySql);

	mysql_autocommit(MySql, 0);  // according to doc, this function never set error(s)
	return true;
}

void xMySqlTransaction::Rollback() {
	auto MySql = (MYSQL *)Conn->GetNative();
	assert(MySql);

	mysql_rollback(MySql);
	// this call never set error(s)
}

bool xMySqlTransaction::Commit() {
	assert(ConnVersion.IsValid());
	Commited = true;

	auto NewConnVersion = Conn->GetVersion();
	if (!NewConnVersion.IsEqualTo(ConnVersion)) {
		X_DEBUG_PRINTF("Transaction connection is changed");
		return false;
	}
	auto MySql = (MYSQL *)Conn->GetNative();
	assert(MySql);
	auto Success = !mysql_commit(MySql);
	ResetAutoCommit();
	return Success;
}

void xMySqlTransaction::ResetAutoCommit() {
	auto MySql = (MYSQL *)Conn->GetNative();
	assert(MySql);
	mysql_autocommit(MySql, 1);
}
