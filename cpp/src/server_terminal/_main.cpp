#include "../common/base.hpp"
#include "./global.hpp"

int main(int argc, char ** argv) {
	auto GG = xScopeGuard(GlobalInit, GlobalClean);

	return 0;
}
