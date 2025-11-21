#include "./mysql_core.hpp"

class xMySqlTransaction final : xNonCopyable {
public:
	xMySqlTransaction(xMySqlConn & ConnRef)
		: Conn(&ConnRef) {
		ConnVersion = ConnRef.GetVersion();
		if (!ConnVersion.IsValid()) {
			X_DEBUG_PRINTF("Failed to start transaction");
			return;
		}
		if (!StartTransaction()) {
			Reset(ConnVersion);
		}
	}

	~xMySqlTransaction() {
		if (!ConnVersion.IsValid()) {
			X_DEBUG_PRINTF("Transaction is incomplete");
			return;
		}
		auto NewConnVersion = Conn->GetVersion();
		if (!NewConnVersion.IsEqualTo(ConnVersion)) {
			X_DEBUG_PRINTF("Transaction connection is changed");
			return;
		}
		if (!Commited) {
			Rollback();
			ResetAutoCommit();
		}
	}

	operator bool() const {
		return ConnVersion.IsValid();
	}

	bool Commit();

private:
	bool StartTransaction();
	void Rollback();
	void ResetAutoCommit();

private:
	xMySqlConn *   Conn        = nullptr;
	xVersionNumber ConnVersion = {};
	bool           Commited    = false;  // set when commition is done, successful or not
};
