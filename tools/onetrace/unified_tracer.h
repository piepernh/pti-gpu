//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ONETRACE_UNIFIED_TRACER_H_
#define PTI_SAMPLES_ONETRACE_UNIFIED_TRACER_H_

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cl_api_collector.h"
#include "cl_kernel_collector.h"
#include "utils.h"
#include "ze_api_collector.h"
#include "ze_kernel_collector.h"

#define ONETRACE_CALL_LOGGING           0
#define ONETRACE_HOST_TIMING            1
#define ONETRACE_DEVICE_TIMING          2
#define ONETRACE_DEVICE_TIMELINE        3
#define ONETRACE_CHROME_DEVICE_TIMELINE 4
#define ONETRACE_CHROME_CALL_LOGGING    5

const char* kChromeTraceFileName = "onetrace.json";

class UnifiedTracer {
 public:
  static UnifiedTracer* Create(unsigned options) {
    cl_device_id cl_cpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_CPU);
    cl_device_id cl_gpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
    if (cl_cpu_device == nullptr && cl_gpu_device == nullptr) {
      std::cerr << "[WARNING] Intel OpenCL devices are not found" << std::endl;
      return nullptr;
    }

    UnifiedTracer* tracer = new UnifiedTracer(options);

    if (tracer->CheckOption(ONETRACE_CALL_LOGGING) ||
        tracer->CheckOption(ONETRACE_CHROME_CALL_LOGGING) ||
        tracer->CheckOption(ONETRACE_HOST_TIMING)) {

      ZeApiCollector* ze_api_collector = nullptr;
      ClApiCollector* cl_cpu_api_collector = nullptr;
      ClApiCollector* cl_gpu_api_collector = nullptr;

      OnZeFunctionFinishCallback ze_callback = nullptr;
      OnClFunctionFinishCallback cl_callback = nullptr;
      if (tracer->CheckOption(ONETRACE_CHROME_CALL_LOGGING)) {
        ze_callback = ChromeLoggingCallback;
        cl_callback = ChromeLoggingCallback;
      }

      bool call_tracing = tracer->CheckOption(ONETRACE_CALL_LOGGING);

      ze_api_collector = ZeApiCollector::Create(
          tracer->start_time_, call_tracing, ze_callback, tracer);
      if (ze_api_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create L0 API collector" <<
          std::endl;
      }
      tracer->ze_api_collector_ = ze_api_collector;

      if (cl_cpu_device != nullptr) {
        cl_cpu_api_collector = ClApiCollector::Create(
            cl_cpu_device, tracer->start_time_,
            call_tracing, cl_callback, tracer);
        if (cl_cpu_api_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create CL API collector for CPU backend" <<
            std::endl;
        }
        tracer->cl_cpu_api_collector_ = cl_cpu_api_collector;
      }

      if (cl_gpu_device != nullptr) {
        cl_gpu_api_collector = ClApiCollector::Create(
            cl_gpu_device, tracer->start_time_,
            call_tracing, cl_callback, tracer);
        if (cl_gpu_api_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create CL API collector for GPU backend" <<
            std::endl;
        }
        tracer->cl_gpu_api_collector_ = cl_gpu_api_collector;
      }

      if (ze_api_collector == nullptr &&
          cl_gpu_api_collector == nullptr &&
          cl_cpu_api_collector == nullptr) {
        delete tracer;
        return nullptr;
      }
    }

    if (tracer->CheckOption(ONETRACE_DEVICE_TIMELINE) ||
        tracer->CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE) ||
        tracer->CheckOption(ONETRACE_DEVICE_TIMING)) {

      ZeKernelCollector* ze_kernel_collector = nullptr;
      ClKernelCollector* cl_cpu_kernel_collector = nullptr;
      ClKernelCollector* cl_gpu_kernel_collector = nullptr;

      OnZeKernelFinishCallback ze_callback = nullptr;
      OnClKernelFinishCallback cl_callback = nullptr;
      if (tracer->CheckOption(ONETRACE_DEVICE_TIMELINE) &&
          tracer->CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE)) {
        ze_callback = DeviceAndChromeTimelineCallback;
        cl_callback = DeviceAndChromeTimelineCallback;
      } else if (tracer->CheckOption(ONETRACE_DEVICE_TIMELINE)) {
        ze_callback = DeviceTimelineCallback;
        cl_callback = DeviceTimelineCallback;
      } else if (tracer->CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE)) {
        ze_callback = ChromeTimelineCallback;
        cl_callback = ChromeTimelineCallback;
      }

      ze_kernel_collector = ZeKernelCollector::Create(
        tracer->start_time_, ze_callback, tracer);
      if (ze_kernel_collector == nullptr) {
        std::cerr <<
          "[WARNING] Unable to create kernel collector for L0 backend" <<
          std::endl;
      }
      tracer->ze_kernel_collector_ = ze_kernel_collector;

      if (cl_cpu_device != nullptr) {
        cl_cpu_kernel_collector = ClKernelCollector::Create(
            cl_cpu_device, tracer->start_time_, cl_callback, tracer);
        if (cl_cpu_kernel_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create kernel collector for CL CPU backend" <<
            std::endl;
        }
        tracer->cl_cpu_kernel_collector_ = cl_cpu_kernel_collector;
      }

      if (cl_gpu_device != nullptr) {
        cl_gpu_kernel_collector = ClKernelCollector::Create(
            cl_gpu_device, tracer->start_time_, cl_callback, tracer);
        if (cl_gpu_kernel_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create kernel collector for CL GPU backend" <<
            std::endl;
        }
        tracer->cl_gpu_kernel_collector_ = cl_gpu_kernel_collector;
      }

      if (ze_kernel_collector == nullptr &&
          cl_cpu_kernel_collector == nullptr &&
          cl_gpu_kernel_collector == nullptr) {
        delete tracer;
        return nullptr;
      }
    }

    return tracer;
  }

  ~UnifiedTracer() {
    std::chrono::steady_clock::time_point end_time =
      std::chrono::steady_clock::now();
    std::chrono::duration<uint64_t, std::nano> duration =
      end_time - start_time_;
    total_execution_time_ = duration.count();

    if (cl_cpu_api_collector_ != nullptr) {
      cl_cpu_api_collector_->DisableTracing();
    }
    if (cl_gpu_api_collector_ != nullptr) {
      cl_gpu_api_collector_->DisableTracing();
    }
    if (ze_api_collector_ != nullptr) {
      ze_api_collector_->DisableTracing();
    }

    if (cl_cpu_kernel_collector_ != nullptr) {
      cl_cpu_kernel_collector_->DisableTracing();
    }
    if (cl_gpu_kernel_collector_ != nullptr) {
      cl_gpu_kernel_collector_->DisableTracing();
    }

    Report();

    if (cl_cpu_api_collector_ != nullptr) {
      delete cl_cpu_api_collector_;
    }
    if (cl_gpu_api_collector_ != nullptr) {
      delete cl_gpu_api_collector_;
    }
    if (ze_api_collector_ != nullptr) {
      delete ze_api_collector_;
    }

    if (cl_cpu_kernel_collector_ != nullptr) {
      delete cl_cpu_kernel_collector_;
    }
    if (cl_gpu_kernel_collector_ != nullptr) {
      delete cl_gpu_kernel_collector_;
    }

    if (chrome_trace_.is_open()) {
      CloseTraceFile();
    }
  }

  bool CheckOption(unsigned option) {
    return (options_ & (1 << option));
  }

  UnifiedTracer(const UnifiedTracer& copy) = delete;
  UnifiedTracer& operator=(const UnifiedTracer& copy) = delete;

 private:
  UnifiedTracer(unsigned options)
      : options_(options) {
    start_time_ = std::chrono::steady_clock::now();

    if (CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE) ||
        CheckOption(ONETRACE_CHROME_CALL_LOGGING)) {
      OpenTraceFile();
    }
  }

  static uint64_t CalculateTotalTime(const ZeApiCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ZeFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
    if (function_info_map.size() != 0) {
      for (auto& value : function_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static uint64_t CalculateTotalTime(const ZeKernelCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ZeKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
    if (kernel_info_map.size() != 0) {
      for (auto& value : kernel_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static uint64_t CalculateTotalTime(const ClApiCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ClFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
    if (function_info_map.size() != 0) {
      for (auto& value : function_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static uint64_t CalculateTotalTime(const ClKernelCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ClKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
    if (kernel_info_map.size() != 0) {
      for (auto& value : kernel_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static void PrintBackendTable(
      const ZeApiCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ZeFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
      PTI_ASSERT(function_info_map.size() > 0);
      ZeApiCollector::PrintFunctionsTable(function_info_map);
    }
  }

  static void PrintBackendTable(
      const ZeKernelCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ZeKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
      PTI_ASSERT(kernel_info_map.size() > 0);
      ZeKernelCollector::PrintKernelsTable(kernel_info_map);
    }
  }

  static void PrintBackendTable(
      const ClApiCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ClFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
      PTI_ASSERT(function_info_map.size() > 0);
      ClApiCollector::PrintFunctionsTable(function_info_map);
    }
  }

  static void PrintBackendTable(
      const ClKernelCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ClKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
      PTI_ASSERT(kernel_info_map.size() > 0);
      ClKernelCollector::PrintKernelsTable(kernel_info_map);
    }
  }

  template <class ZeCollector, class ClCollector>
  void ReportTiming(
      const ZeCollector* ze_collector,
      const ClCollector* cl_cpu_collector,
      const ClCollector* cl_gpu_collector,
      const char* type) {
    PTI_ASSERT (cl_cpu_collector != nullptr || cl_gpu_collector != nullptr);

    std::string ze_title =
      std::string("Total ") + std::string(type) +
      " Time for L0 backend (ns): ";
    std::string cl_cpu_title =
      std::string("Total ") + std::string(type) +
      " Time for CL CPU backend (ns): ";
    std::string cl_gpu_title =
      std::string("Total ") + std::string(type) +
      " Time for CL GPU backend (ns): ";
    size_t title_width = std::max(cl_cpu_title.size(), cl_gpu_title.size());
    title_width = std::max(title_width, ze_title.size());
    const size_t time_width = 20;

    std::cerr << std::endl;
    std::cerr << "=== " << type << " Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << std::setw(title_width) << "Total Execution Time (ns): " <<
      std::setw(time_width) << total_execution_time_ << std::endl;

    if (ze_collector != nullptr) {
      uint64_t total_time = CalculateTotalTime(ze_collector);
      if (total_time > 0) {
        std::cerr << std::setw(title_width) << ze_title <<
          std::setw(time_width) << total_time <<
          std::endl;
      }
    }
    if (cl_cpu_collector != nullptr) {
      uint64_t total_time = CalculateTotalTime(cl_cpu_collector);
      if (total_time > 0) {
        std::cerr << std::setw(title_width) << cl_cpu_title <<
          std::setw(time_width) << total_time <<
          std::endl;
      }
    }
    if (cl_gpu_collector != nullptr) {
      uint64_t total_time = CalculateTotalTime(cl_gpu_collector);
      if (total_time > 0) {
        std::cerr << std::setw(title_width) << cl_gpu_title <<
          std::setw(time_width) << total_time <<
          std::endl;
      }
    }

    if (ze_collector != nullptr) {
      PrintBackendTable(ze_collector, "L0");
    }
    if (cl_cpu_collector != nullptr) {
      PrintBackendTable(cl_cpu_collector, "CL CPU");
    }
    if (cl_gpu_collector != nullptr) {
      PrintBackendTable(cl_gpu_collector, "CL GPU");
    }

    std::cerr << std::endl;
  }

  void Report() {
    if (CheckOption(ONETRACE_HOST_TIMING)) {
      ReportTiming(
          ze_api_collector_,
          cl_cpu_api_collector_,
          cl_gpu_api_collector_,
          "API");
    }
    if (CheckOption(ONETRACE_DEVICE_TIMING)) {
      ReportTiming(
          ze_kernel_collector_,
          cl_cpu_kernel_collector_,
          cl_gpu_kernel_collector_,
          "Device");
    }
    std::cerr << std::endl;
  }

  static void DeviceTimelineCallback(
      void* data, void* queue, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    std::stringstream stream;
    stream << "Device Timeline (queue: " << queue <<
      "): " << name << " [ns] = " <<
      queued << " (queued) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;
    std::cerr << stream.str();
  }

  void OpenTraceFile() {
    chrome_trace_.open(kChromeTraceFileName);
    PTI_ASSERT(chrome_trace_.is_open());
    chrome_trace_ << "[" << std::endl;
    chrome_trace_ <<
      "{\"ph\":\"M\", \"name\":\"process_name\", \"pid\":" <<
      utils::GetPid() << ", \"tid\":0, \"args\":{\"name\":\"" <<
      utils::GetExecutableName() << "\"}}," << std::endl;
  }

  void CloseTraceFile() {
    PTI_ASSERT(chrome_trace_.is_open());
    chrome_trace_.close();
    std::cerr << "Timeline was stored to " <<
      kChromeTraceFileName << std::endl;
  }

  static void ChromeTimelineCallback(
      void* data, void* queue, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":" << utils::GetPid() <<
      ", \"tid\":" << reinterpret_cast<uint64_t>(queue) <<
      ", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      "}," << std::endl;
    tracer->chrome_trace_ << stream.str();
  }

  static void DeviceAndChromeTimelineCallback(
      void* data, void* queue, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    DeviceTimelineCallback(data, queue, name, queued, submitted, started, ended);
    ChromeTimelineCallback(data, queue, name, queued, submitted, started, ended);
  }

  static void ChromeLoggingCallback(
      void* data, const std::string& name,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":" <<
      utils::GetPid() << ", \"tid\":" << utils::GetTid() <<
      ", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      "}," << std::endl;
    tracer->chrome_trace_ << stream.str();
  }

 private:
  unsigned options_;

  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  uint64_t total_execution_time_ = 0;

  ZeApiCollector* ze_api_collector_ = nullptr;
  ClApiCollector* cl_cpu_api_collector_ = nullptr;
  ClApiCollector* cl_gpu_api_collector_ = nullptr;

  ZeKernelCollector* ze_kernel_collector_ = nullptr;
  ClKernelCollector* cl_cpu_kernel_collector_ = nullptr;
  ClKernelCollector* cl_gpu_kernel_collector_ = nullptr;

  std::ofstream chrome_trace_;
};

#endif // PTI_SAMPLES_ONETRACE_UNIFIED_TRACER_H_