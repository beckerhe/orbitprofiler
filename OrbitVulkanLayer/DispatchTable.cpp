// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DispatchTable.h"

namespace orbit_vulkan_layer {

void DispatchTable::CreateInstanceDispatchTable(
    VkInstance instance, PFN_vkGetInstanceProcAddr next_get_instance_proc_addr_function) {
  VkLayerInstanceDispatchTable dispatch_table;
  dispatch_table.DestroyInstance = absl::bit_cast<PFN_vkDestroyInstance>(
      next_get_instance_proc_addr_function(instance, "vkDestroyInstance"));
  dispatch_table.GetInstanceProcAddr = absl::bit_cast<PFN_vkGetInstanceProcAddr>(
      next_get_instance_proc_addr_function(instance, "vkGetInstanceProcAddr"));
  dispatch_table.EnumerateDeviceExtensionProperties =
      absl::bit_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          next_get_instance_proc_addr_function(instance, "vkEnumerateDeviceExtensionProperties"));
  dispatch_table.GetPhysicalDeviceProperties = absl::bit_cast<PFN_vkGetPhysicalDeviceProperties>(
      next_get_instance_proc_addr_function(instance, "vkGetPhysicalDeviceProperties"));

  void* key = GetDispatchTableKey(instance);
  {
    absl::WriterMutexLock lock(&mutex_);
    CHECK(!instance_dispatch_table_.contains(key));
    instance_dispatch_table_[key] = dispatch_table;
  }
}

void DispatchTable::RemoveInstanceDispatchTable(VkInstance instance) {
  void* key = GetDispatchTableKey(instance);
  {
    absl::WriterMutexLock lock(&mutex_);
    CHECK(instance_dispatch_table_.contains(key));
    instance_dispatch_table_.erase(key);
  }
}

void DispatchTable::CreateDeviceDispatchTable(
    VkDevice device, PFN_vkGetDeviceProcAddr next_get_device_proc_addr_function) {
  VkLayerDispatchTable dispatch_table;

  dispatch_table.DestroyDevice = absl::bit_cast<PFN_vkDestroyDevice>(
      next_get_device_proc_addr_function(device, "vkDestroyDevice"));
  dispatch_table.GetDeviceProcAddr = absl::bit_cast<PFN_vkGetDeviceProcAddr>(
      next_get_device_proc_addr_function(device, "vkGetDeviceProcAddr"));

  dispatch_table.ResetCommandPool = absl::bit_cast<PFN_vkResetCommandPool>(
      next_get_device_proc_addr_function(device, "vkResetCommandPool"));

  dispatch_table.AllocateCommandBuffers = absl::bit_cast<PFN_vkAllocateCommandBuffers>(
      next_get_device_proc_addr_function(device, "vkAllocateCommandBuffers"));
  dispatch_table.FreeCommandBuffers = absl::bit_cast<PFN_vkFreeCommandBuffers>(
      next_get_device_proc_addr_function(device, "vkFreeCommandBuffers"));
  dispatch_table.BeginCommandBuffer = absl::bit_cast<PFN_vkBeginCommandBuffer>(
      next_get_device_proc_addr_function(device, "vkBeginCommandBuffer"));
  dispatch_table.EndCommandBuffer = absl::bit_cast<PFN_vkEndCommandBuffer>(
      next_get_device_proc_addr_function(device, "vkEndCommandBuffer"));
  dispatch_table.ResetCommandBuffer = absl::bit_cast<PFN_vkResetCommandBuffer>(
      next_get_device_proc_addr_function(device, "vkResetCommandBuffer"));

  dispatch_table.QueueSubmit = absl::bit_cast<PFN_vkQueueSubmit>(
      next_get_device_proc_addr_function(device, "vkQueueSubmit"));
  dispatch_table.QueuePresentKHR = absl::bit_cast<PFN_vkQueuePresentKHR>(
      next_get_device_proc_addr_function(device, "vkQueuePresentKHR"));

  dispatch_table.GetDeviceQueue = absl::bit_cast<PFN_vkGetDeviceQueue>(
      next_get_device_proc_addr_function(device, "vkGetDeviceQueue"));
  dispatch_table.GetDeviceQueue2 = absl::bit_cast<PFN_vkGetDeviceQueue2>(
      next_get_device_proc_addr_function(device, "vkGetDeviceQueue2"));

  dispatch_table.CreateQueryPool = absl::bit_cast<PFN_vkCreateQueryPool>(
      next_get_device_proc_addr_function(device, "vkCreateQueryPool"));
  dispatch_table.ResetQueryPoolEXT = absl::bit_cast<PFN_vkResetQueryPoolEXT>(
      next_get_device_proc_addr_function(device, "vkResetQueryPoolEXT"));

  dispatch_table.CmdWriteTimestamp = absl::bit_cast<PFN_vkCmdWriteTimestamp>(
      next_get_device_proc_addr_function(device, "vkCmdWriteTimestamp"));

  dispatch_table.GetQueryPoolResults = absl::bit_cast<PFN_vkGetQueryPoolResults>(
      next_get_device_proc_addr_function(device, "vkGetQueryPoolResults"));

  dispatch_table.CmdBeginDebugUtilsLabelEXT = absl::bit_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
      next_get_device_proc_addr_function(device, "vkCmdBeginDebugUtilsLabelEXT"));
  dispatch_table.CmdEndDebugUtilsLabelEXT = absl::bit_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
      next_get_device_proc_addr_function(device, "vkCmdEndDebugUtilsLabelEXT"));

  dispatch_table.CmdDebugMarkerBeginEXT = absl::bit_cast<PFN_vkCmdDebugMarkerBeginEXT>(
      next_get_device_proc_addr_function(device, "vkCmdDebugMarkerBeginEXT"));
  dispatch_table.CmdDebugMarkerEndEXT = absl::bit_cast<PFN_vkCmdDebugMarkerEndEXT>(
      next_get_device_proc_addr_function(device, "vkCmdDebugMarkerEndEXT"));

  void* key = GetDispatchTableKey(device);
  {
    absl::WriterMutexLock lock(&mutex_);
    CHECK(!device_dispatch_table_.contains(key));
    device_dispatch_table_[key] = dispatch_table;

    CHECK(!device_supports_debug_utils_extension_.contains(key));
    device_supports_debug_utils_extension_[key] =
        dispatch_table.CmdBeginDebugUtilsLabelEXT != nullptr &&
        dispatch_table.CmdEndDebugUtilsLabelEXT != nullptr;

    CHECK(!device_supports_debug_marker_extension_.contains(key));
    device_supports_debug_marker_extension_[key] =
        dispatch_table.CmdDebugMarkerBeginEXT != nullptr &&
        dispatch_table.CmdDebugMarkerEndEXT != nullptr;
  }
}

void DispatchTable::RemoveDeviceDispatchTable(VkDevice device) {
  void* key = GetDispatchTableKey(device);
  {
    absl::WriterMutexLock lock(&mutex_);

    CHECK(device_dispatch_table_.contains(key));
    device_dispatch_table_.erase(key);

    CHECK(device_supports_debug_utils_extension_.contains(key));
    device_supports_debug_utils_extension_.erase(key);

    CHECK(device_supports_debug_marker_extension_.contains(key));
    device_supports_debug_marker_extension_.erase(key);
  }
}

}  // namespace orbit_vulkan_layer