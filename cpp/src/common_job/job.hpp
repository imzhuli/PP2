#pragma once
#include "../common/base.hpp"

struct xJobNode
	: xListNode
	, xVBase {
	uint64_t JobId;
	uint64_t JobCreateTimestampMS;
};
using xJobList = xList<xJobNode>;

class xJobWheel {
public:
	bool Init(size_t MaxTicks) {
		this->CurrentIndex = 0;
		this->MaxTicks     = MaxTicks;
		WheelNodes.resize(MaxTicks + 1);
		return true;
	}

	void Clean() {
		CleanJobs(AssertUnfinishedJobs, {});
		Renew(WheelNodes);
	}

	size_t GetMaxTicks() const {
		return MaxTicks;
	}

	xJobList & GetImmediateJobList() {
		return WheelNodes[CurrentIndex];
	}

	void Forward() {
		CurrentIndex = (CurrentIndex + 1) % WheelNodes.size();
	}

	void DeferJob(xJobNode & J, size_t Ticks = 0) {
		assert(Ticks < MaxTicks);
		auto JobIndex = (CurrentIndex + Ticks) % WheelNodes.size();
		WheelNodes[JobIndex].GrabTail(J);
	}

	void ProcessJobs(void (*Callback)(xVariable, xJobNode &), xVariable V = {}, size_t Ticks = 1) {
		assert(Ticks <= MaxTicks);
		for (size_t i = 0; i < Ticks; ++i) {
			auto & List = WheelNodes[CurrentIndex];
			for (auto & N : List) {
				List.Remove(N);
				Callback(V, N);
			}
			CurrentIndex = (CurrentIndex + 1) % WheelNodes.size();
		}
	}

	void CleanJobs(void (*Callback)(xVariable, xJobNode &), xVariable V) {
		ProcessJobs(Callback, V, WheelNodes.size());
	}

private:
	static void AssertUnfinishedJobs(xVariable, xJobNode &) {
		X_PERROR("AssertUnfinishedJobs");
		Fatal();
	}

private:
	size_t                CurrentIndex;
	size_t                MaxTicks;
	std::vector<xJobList> WheelNodes;
};

class xJobPool {
public:
	// from other threads:
	void       PostWakeupN(int64_t N);
	void       PostJob(xJobNode & Job);
	xJobNode * WaitForJob();
	xJobNode * WaitForJobTimeout(uint64_t MS);

private:
	xJobList   JobList;
	xSemaphore JobSemaphore;
};