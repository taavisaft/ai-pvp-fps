#include "net_common.h"
#include <cstring>

bool netOpen(int& fd) {
    fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;
    setNonBlocking(fd);
    return true;
}

bool netBind(int fd, uint16_t port) {
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    return bind(fd, (sockaddr*)&addr, sizeof(addr)) == 0;
}

bool netResolve(const char* ip, uint16_t port, sockaddr_in& out) {
    memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port   = htons(port);
    return inet_pton(AF_INET, ip, &out.sin_addr) == 1;
}

bool netSend(int fd, const void* buf, int len, const sockaddr_in& to) {
    return sendto(fd, (const char*)buf, len, 0,
                  (const sockaddr*)&to, sizeof(to)) == len;
}

int netRecv(int fd, void* buf, int len, sockaddr_in& from) {
    socklen_t fromLen = sizeof(from);
    int n = (int)recvfrom(fd, (char*)buf, len, 0, (sockaddr*)&from, &fromLen);
    return n > 0 ? n : -1;
}

bool netSameAddr(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}
