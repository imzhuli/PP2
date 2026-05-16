#include <pp_common/relay/relay_id.hpp>

static xRelayIdManager RIM;

int main(int, char **) {

    for (int I = 0; I < 10; ++I) {
        auto Id = RIM.Acquire(eRelayServerType::DEVICE);
        printf("%" PRIu64 ", %s\n", Id, YN(RIM.Validate(Id)));
    }

    return 0;
}
