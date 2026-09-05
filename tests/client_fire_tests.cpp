#include "network.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

int main() {
    ClientNet net;
    StatePacket state{};
    state.type = PKT_STATE;
    state.seq = 10;
    state.fireEpoch = 100;
    state.usedMask = 1;
    auto receive = [&]() {
        net.processPacket(reinterpret_cast<const char*>(&state), statePacketSize(0), net.server);
    };
    net.shotSeq = 9;
    receive();
    CHECK(net.hasState && net.shotSeq == 0); // fresh session has no inherited requests
    net.shotSeq = 3;
    state.seq = 11;
    receive();
    CHECK(net.shotSeq == 3); // ordinary snapshots must not create duplicate shot IDs
    state.seq = 12;
    state.fireEpoch = 101;
    receive();
    CHECK(net.shotSeq == 0); // new epoch starts a fresh counter, old shots aren't relabeled
    net.shotSeq = 2;
    state.seq = 10;
    state.fireEpoch = 100;
    receive();
    CHECK(net.shotSeq == 2 && net.lastState.fireEpoch == 101); // delayed state cannot roll back epoch
    state.seq = 13;
    state.fireEpoch = 102;
    net.processPacket(reinterpret_cast<const char*>(&state), statePacketSize(0) - 1, net.server);
    CHECK(net.shotSeq == 2); // truncated packet cannot erase requests
    receive();
    CHECK(net.shotSeq == 0 && net.lastState.fireEpoch == 102);
    if (failures) return 1;
    std::puts("client fire tests passed: epoch reset, duplicates, stale and truncated snapshots");
}
