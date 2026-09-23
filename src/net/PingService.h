#pragma once
#include <WiFiClient.h>

int tcpPing(const char* host, uint16_t port, uint32_t timeout);
