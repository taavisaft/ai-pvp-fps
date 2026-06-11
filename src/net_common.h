#pragma once
#include "platform.h"
#include <cstdint>

// Thin helpers over BSD/Winsock UDP. All non-blocking.
bool netOpen(int& fd);                                           // create + set non-blocking
bool netBind(int fd, uint16_t port);                             // SO_REUSEADDR + bind
bool netResolve(const char* ip, uint16_t port, sockaddr_in& out);
bool netSend(int fd, const void* buf, int len, const sockaddr_in& to);
int  netRecv(int fd, void* buf, int len, sockaddr_in& from);     // bytes, or -1 if none
bool netSameAddr(const sockaddr_in& a, const sockaddr_in& b);
