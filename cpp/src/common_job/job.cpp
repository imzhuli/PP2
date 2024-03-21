#include "./job.hpp"

// from other threads:
void xJobPool::PostWakeupN(int64_t N) {
	JobSemaphore.NotifyN(N);
}

void xJobPool::PostJob(xJobNode & Job) {
	JobSemaphore.Notify([this, &Job] { JobList.GrabTail(Job); });
}

xJobNode * xJobPool::WaitForJob() {
	xJobNode * R = nullptr;
	JobSemaphore.Wait([this, &R] { R = JobList.PopHead(); });
	return R;
}

xJobNode * xJobPool::WaitForJobTimeout(uint64_t MS) {
	xJobNode * R = nullptr;
	JobSemaphore.WaitFor(xMilliSeconds(MS), [this, &R] { R = JobList.PopHead(); });
	return R;
}
