#pragma once

void startWebServer();

// Runs deferred actions (reboot, factory reset) requested from handlers.
// Call from loop().
void webLoop();
