#pragma once
#include "../common/base.hpp"
#include "../common_job/job.hpp"

struct xDnsJob : xJobNode {
	xVariable   JobCtx = {};
	std::string Hostname;
	xNetAddress A4;
	xNetAddress A6;
};

extern xJobQueue NewDnsJobQueue;
extern xJobList  FinishedJobList;
extern xSpinlock FinishedJobListLock;

xDnsJob * NewDnsJob(const std::string & Hostname, xVariable JobCtx = {});
void      DeleteDnsJob(const xDnsJob * PJ);

void DnsWorkerThread(xNotifier Notifier, xVariable = {});
