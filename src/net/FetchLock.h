#pragma once

// One outgoing HTTP request at a time. A TLS session holds about 32 KB of
// internal RAM while it is open, and the data source task and the image
// task each used to open their own: two at once took most of what is free.
// Hold a FetchGuard for as long as the connection exists.
struct FetchGuard {
  FetchGuard();
  ~FetchGuard();
  FetchGuard(const FetchGuard&) = delete;
  FetchGuard& operator=(const FetchGuard&) = delete;
};
