//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>

#include "unified_tracer.h"

static UnifiedTracer* tracer = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./onetrace[.exe] [options] <application> <args>" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--call-logging [-c]             Trace host API calls" <<
    std::endl;
  std::cout <<
    "--host-timing  [-h]             Report host API execution time" <<
    std::endl;
  std::cout <<
    "--device-timing [-d]            Report kernels execution time" <<
    std::endl;
  std::cout <<
    "--device-timeline [-t]          Trace device activities" <<
    std::endl;
  std::cout <<
    "--chrome-device-timeline        Dump device activities to JSON file" <<
    std::endl;
  std::cout <<
    "--chrome-call-logging           Dump host API calls to JSON file" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  int app_index = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--call-logging") == 0 ||
        strcmp(argv[i], "-c") == 0) {
      utils::SetEnv("ONETRACE_CallLogging=1");
      ++app_index;
    } else if (strcmp(argv[i], "--host-timing") == 0 ||
               strcmp(argv[i], "-h") == 0) {
      utils::SetEnv("ONETRACE_HostTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      utils::SetEnv("ONETRACE_DeviceTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timeline") == 0 ||
               strcmp(argv[i], "-t") == 0) {
      utils::SetEnv("ONETRACE_DeviceTimeline=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-timeline") == 0) {
      utils::SetEnv("ONETRACE_ChromeDeviceTimeline=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-call-logging") == 0) {
      utils::SetEnv("ONETRACE_ChromeCallLogging=1");
      ++app_index;
    } else {
      break;
    }
  }
  return app_index;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER=1");
}

static unsigned ReadArgs() {
  std::string value;
  unsigned options = 0;

  value = utils::GetEnv("ONETRACE_CallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << ONETRACE_CALL_LOGGING);
  }

  value = utils::GetEnv("ONETRACE_HostTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << ONETRACE_HOST_TIMING);
  }

  value = utils::GetEnv("ONETRACE_DeviceTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << ONETRACE_DEVICE_TIMING);
  }

  value = utils::GetEnv("ONETRACE_DeviceTimeline");
  if (!value.empty() && value == "1") {
    options |= (1 << ONETRACE_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("ONETRACE_ChromeDeviceTimeline");
  if (!value.empty() && value == "1") {
    options |= (1 << ONETRACE_CHROME_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("ONETRACE_ChromeCallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << ONETRACE_CHROME_CALL_LOGGING);
  }

  return options;
}

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  unsigned options = ReadArgs();
  if (options == 0) {
    options |= (1 << ONETRACE_HOST_TIMING);
    options |= (1 << ONETRACE_DEVICE_TIMING);
  }

  tracer = UnifiedTracer::Create(options);
}

void DisableProfiling() {
  if (tracer != nullptr) {
    delete tracer;
  }
}