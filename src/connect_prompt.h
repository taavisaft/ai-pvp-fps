#pragma once
#include <cstdio>
#include <cstring>

// In-game server-IP entry overlay (C key). Avoids blocking stdin while SDL owns the window.
struct ConnectPrompt {
    bool open = false;
    char ip[64] = {};
    int  len    = 0;

    void show(const char* defaultIp) {
        snprintf(ip, sizeof(ip), "%s", defaultIp);
        len = (int)strlen(ip);
        open = true;
    }

    void close() { open = false; }

    void append(char c) {
        if (len >= 63) return;
        if (!((c >= '0' && c <= '9') || c == '.')) return;
        ip[len++] = c;
        ip[len] = '\0';
    }

    void backspace() {
        if (len > 0) ip[--len] = '\0';
    }
};
