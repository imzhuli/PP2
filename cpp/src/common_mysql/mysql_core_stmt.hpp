#pragma once
#include "./mysql_core.hpp"
#include "./mysql_core_bind.hpp"

class xMySqlStmt final {

public:
	bool Init(xMySqlConn & MySqlConn, const std::string & Query);
	void Clean();

protected:
	bool CheckReonnection();
	void Open();
	void Close();
	void TestAndClose();
	bool ProcessServerLostError();  // if return value is true, a server lost is found;

	// in bind
	static void InternalSetupInBind(void * BindPtr, int Type, void * BufferPtr, unsigned long Length);
	template <typename T>
	static void SetupExactInBind(void * BindPtr, xInBind<T> & B) {
		InternalSetupInBind(BindPtr, B.GetTypeEnum(), B.GetSourcePtr(), B.GetSourceSize());
	}

	static void SetupInBind(void * BindPtr) {
	}

	template <typename tArg0, typename... tArgs>
	static void SetupInBind(void * BindPtr, xInBind<tArg0> & Arg0, xInBind<tArgs> &... Args) {
		SetupExactInBind(BindPtr, Arg0);
		SetupInBind(NextBind(BindPtr), Args...);
	}

public:
	using xOnRowRecord        = void (*)();
	using xOnRowRecordWithCtx = void (*)(xVariable);

	template <typename... tRArgs, typename... tArgs>
	bool Execute(const std::tuple<tRArgs &...> & R, tArgs &&... Args) {
		return ExecuteAndGetIndirect(Ignore, {}, R, xInBind(std::forward<tArgs>(Args))...);
	}

	template <typename... tRArgs, typename... tArgs>
	bool Execute(xOnRowRecord Callback, const std::tuple<tRArgs &...> & R, tArgs &&... Args) {
		return ExecuteAndGetIndirect(&OnRowDelegate, { .FP = F2H(Callback) }, R, xInBind(std::forward<tArgs>(Args))...);
	}

	template <typename... tRArgs, typename... tArgs>
	bool Execute(xOnRowRecordWithCtx Callback, xVariable UserCtx, const std::tuple<tRArgs &...> & R, tArgs &&... Args) {
		return ExecuteAndGetIndirect(Callback, UserCtx, R, xInBind(std::forward<tArgs>(Args))...);
	}

	uint64_t GetLastInsertId() const {
		return LastInsertId;
	}
	size_t GetLastResultRows() const {
		return LastResultRows;
	}
	size_t GetLastAffectedRows() const {
		return LastAffectedRows;
	}

protected:
	bool InternalBindAndExecute(xOnRowRecordWithCtx Callback, xVariable UserCtx, void * InputBind, void * OutputBind, xOutBind * OutputBindDelegate);

	template <typename... tRArgs, typename... tArgs>
	bool ExecuteAndGetIndirect(xOnRowRecordWithCtx Callback, xVariable UserCtx, const std::tuple<tRArgs &...> & R, xInBind<tArgs> &&... Args) {
		if (!CheckReonnection()) {
			return false;
		}
		size_t TotalValues = sizeof...(tRArgs) + sizeof...(Args);
		auto   BindPtr     = AllocBinds(TotalValues);
		auto   RBindPtr    = NextBindN(BindPtr, sizeof...(Args));
		auto   BindPtrG    = xScopeGuard([&] { FreeBinds(BindPtr); });
		SetupInBind(BindPtr, Args...);
		auto OutBindArray    = MakeOutBindIndirect(RBindPtr, R);
		auto OutBindDelegate = sizeof...(tRArgs) ? &OutBindArray[0] : nullptr;
		if (!InternalBindAndExecute(Callback, UserCtx, BindPtr, RBindPtr, OutBindDelegate)) {
			return false;
		}
		return true;
	}

	static void Ignore(xVariable UserCtx) {
		Pass();
	}
	static void OnRowDelegate(xVariable RealOnRow) {
		reinterpret_cast<xOnRowRecord>(RealOnRow.FP)();
	}

private:
	xMySqlConn *   Conn;
	xVersionNumber ConnVersion;
	std::string    Query;
	void *         Native;

	// Runtime Temps:
	uint64_t LastInsertId;
	size_t   LastResultRows;
	size_t   LastAffectedRows;
};
