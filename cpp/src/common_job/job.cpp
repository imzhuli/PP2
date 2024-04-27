#include "./job.hpp"

using namespace xel;

// from other threads:
void xJobQueue::PostWakeupN(int64_t N) {
	JobSemaphore.NotifyN(N);
}

void xJobQueue::PostJob(xJobNode & Job) {
	JobSemaphore.Notify([this, &Job] { JobList.GrabTail(Job); });
}

void xJobQueue::GrabJobList(xJobList & DstJobList) {
	JobSemaphore.Reset([this, &DstJobList] { DstJobList.GrabListTail(JobList); });
}

xJobNode * xJobQueue::WaitForJob() {
	xJobNode * R = nullptr;
	JobSemaphore.Wait([this, &R] { R = JobList.PopHead(); });
	return R;
}

xJobNode * xJobQueue::WaitForJobTimeout(uint64_t MS) {
	xJobNode * R = nullptr;
	JobSemaphore.WaitFor(xMilliSeconds(MS), [this, &R] { R = JobList.PopHead(); });
	return R;
}
