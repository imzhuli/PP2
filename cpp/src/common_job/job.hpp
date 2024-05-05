#pragma once
#include <core/core_min.hpp>
#include <core/core_time.hpp>
#include <core/list.hpp>
#include <core/thread.hpp>
#include <vector>

struct xJobNode : xel::xListNode {
protected:
	X_INLINE ~xJobNode() = default;
};
using xJobList = xel::xList<xJobNode>;

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

	void ProcessJobs(void (*Callback)(xel::xVariable, xJobNode &), xel::xVariable V = {}, size_t Ticks = 1) {
		assert(Ticks <= MaxTicks);
		for (size_t i = 0; i < Ticks; ++i) {
			auto & List = WheelNodes[CurrentIndex];
			while (auto NP = List.PopHead()) {
				Callback(V, *NP);
			}
			CurrentIndex = (CurrentIndex + 1) % WheelNodes.size();
		}
	}

	void CleanJobs(void (*Callback)(xel::xVariable, xJobNode &), xel::xVariable V) {
		ProcessJobs(Callback, V, WheelNodes.size());
	}

private:
	static void AssertUnfinishedJobs(xel::xVariable, xJobNode &) {
		X_PFATAL("AssertUnfinishedJobs");
	}

private:
	size_t                CurrentIndex;
	size_t                MaxTicks;
	std::vector<xJobList> WheelNodes;
};

class xJobQueue {
public:
	// from other threads:
	void       PostWakeupN(int64_t N);
	void       PostJob(xJobNode & Job);
	void       GrabJobList(xJobList & DstJobList);
	xJobNode * WaitForJob();
	xJobNode * WaitForJobTimeout(uint64_t MS);

	template <typename T>
	std::enable_if_t<std::is_base_of_v<xJobNode, T>, bool> WaitForJobTimeout(T *& R, uint64_t MS) {
		return JobSemaphore.WaitFor(xel::xMilliSeconds(MS), [this, &R] { R = static_cast<T *>(JobList.PopHead()); });
	}

private:
	xJobList        JobList;
	xel::xSemaphore JobSemaphore;
};
