#pragma once
#include "../common/base.hpp"
#include "../common_job/job.hpp"

struct xDnsJob : xJobNode {
	uint64_t    JobId = 0;
	std::string Hostname;
	xNetAddress A4;
	xNetAddress A6;
};

extern xJobQueue NewDnsJobQueue;
extern xJobList  FinishedJobList;
extern xSpinlock FinishedJobListLock;

xDnsJob * NewDnsJob(const std::string & Hostname, uint64_t JobId = 0);
void      DeleteDnsJob(const xDnsJob * PJ);

void DnsQueryThread();
