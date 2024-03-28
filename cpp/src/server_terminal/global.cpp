#include "./global.hpp"

xIoContext IoCtx;

void GlobalInit() {
	RuntimeAssert(IoCtx.Init());
}

void GlobalClean() {
	IoCtx.Clean();
}
