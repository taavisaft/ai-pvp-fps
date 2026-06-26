#pragma once
#include <cstdio>
#include <cstring>

// In-game connection overlay (C key). Two stages: PM_IP types a host address, then
// PM_BROWSE lists the map servers the lobby probe found on that host so you pick one.
// Avoids blocking stdin while SDL owns the window.
enum PromptMode { PM_IP = 0, PM_BROWSE = 1 };

struct ConnectPrompt {
    bool open = false;
    int  mode = PM_IP;
    char ip[64] = {};
    int  len    = 0;
    int  sel    = 0;        // highlighted server row while PM_BROWSE

    void show(const char* defaultIp) {
        snprintf(ip, sizeof(ip), "%s", defaultIp);
        len  = (int)strlen(ip);
        mode = PM_IP;
        sel  = 0;
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
