#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socklen_t = int;
  inline void platformSocketInit()    { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); }
  inline void platformSocketCleanup() { WSACleanup(); }
  inline void setNonBlocking(int fd)  { u_long m = 1; ioctlsocket(fd, FIONBIO, &m); }
  inline void closeSocket(int fd)     { closesocket(fd); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <unistd.h>
  inline void platformSocketInit()    {}
  inline void platformSocketCleanup() {}
  inline void setNonBlocking(int fd)  { fcntl(fd, F_SETFL, O_NONBLOCK); }
  inline void closeSocket(int fd)     { close(fd); }
#endif
