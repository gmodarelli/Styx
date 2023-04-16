/*
Copyright(c) 2023 Giuseppe Modarelli

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "pch.h"
#include "DX12.h"

#include "../Core/Window.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>

#if defined(DEBUG)
#include <dxgidebug.h>
#endif

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 608; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace
{
	// Taken from Panos Karabelas' SpartanEngine
    inline const char* HResultToString(const HRESULT error_code)
    {
        switch (error_code)
        {
            case DXGI_ERROR_DEVICE_HUNG:                   return "DXGI_ERROR_DEVICE_HUNG";                   // The application's device failed due to badly formed commands sent by the application. This is an design-time issue that should be investigated and fixed.
            case DXGI_ERROR_DEVICE_REMOVED:                return "DXGI_ERROR_DEVICE_REMOVED";                // The video card has been physically removed from the system, or a driver upgrade for the video card has occurred. The application should destroy and recreate the device. For help debugging the problem, call ID3D10Device::GetDeviceRemovedReason.
            case DXGI_ERROR_DEVICE_RESET:                  return "DXGI_ERROR_DEVICE_RESET";                  // The device failed due to a badly formed command. This is a run-time issue; The application should destroy and recreate the device.
            case DXGI_ERROR_DRIVER_INTERNAL_ERROR:         return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";         // The driver encountered a problem and was put into the device removed state.
            case DXGI_ERROR_FRAME_STATISTICS_DISJOINT:     return "DXGI_ERROR_FRAME_STATISTICS_DISJOINT";     // An event (for example, a power cycle) interrupted the gathering of presentation statistics.
            case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE:  return "DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE";  // The application attempted to acquire exclusive ownership of an output, but failed because some other application (or device within the application) already acquired ownership.
            case DXGI_ERROR_INVALID_CALL:                  return "DXGI_ERROR_INVALID_CALL";                  // The application provided invalid parameter data; this must be debugged and fixed before the application is released.
            case DXGI_ERROR_MORE_DATA:                     return "DXGI_ERROR_MORE_DATA";                     // The buffer supplied by the application is not big enough to hold the requested data.
            case DXGI_ERROR_NONEXCLUSIVE:                  return "DXGI_ERROR_NONEXCLUSIVE";                  // A global counter resource is in use, and the Direct3D device can't currently use the counter resource.
            case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:       return "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE";       // The resource or request is not currently available, but it might become available later.
            case DXGI_ERROR_NOT_FOUND:                     return "DXGI_ERROR_NOT_FOUND";                     // When calling IDXGIObject::GetPrivateData, the GUID passed in is not recognized as one previously passed to IDXGIObject::SetPrivateData or IDXGIObject::SetPrivateDataInterface. When calling IDXGIFentityy::EnumAdapters or IDXGIAdapter::EnumOutputs, the enumerated ordinal is out of range.
            case DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED:    return "DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED";    // Reserved
            case DXGI_ERROR_REMOTE_OUTOFMEMORY:            return "DXGI_ERROR_REMOTE_OUTOFMEMORY";            // Reserved
            case DXGI_ERROR_WAS_STILL_DRAWING:             return "DXGI_ERROR_WAS_STILL_DRAWING";             // The GPU was busy at the moment when a call was made to perform an operation, and did not execute or schedule the operation.
            case DXGI_ERROR_UNSUPPORTED:                   return "DXGI_ERROR_UNSUPPORTED";                   // The requested functionality is not supported by the device or the driver.
            case DXGI_ERROR_ACCESS_LOST:                   return "DXGI_ERROR_ACCESS_LOST";                   // The desktop duplication interface is invalid. The desktop duplication interface typically becomes invalid when a different type of image is displayed on the desktop.
            case DXGI_ERROR_WAIT_TIMEOUT:                  return "DXGI_ERROR_WAIT_TIMEOUT";                  // The time-out interval elapsed before the next desktop frame was available.
            case DXGI_ERROR_SESSION_DISCONNECTED:          return "DXGI_ERROR_SESSION_DISCONNECTED";          // The Remote Desktop Services session is currently disconnected.
            case DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE:      return "DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE";      // The DXGI output (monitor) to which the swap chain content was restricted is now disconnected or changed.
            case DXGI_ERROR_CANNOT_PROTECT_CONTENT:        return "DXGI_ERROR_CANNOT_PROTECT_CONTENT";        // DXGI can't provide content protection on the swap chain. This error is typically caused by an older driver, or when you use a swap chain that is incompatible with content protection.
            case DXGI_ERROR_ACCESS_DENIED:                 return "DXGI_ERROR_ACCESS_DENIED";                 // You tried to use a resource to which you did not have the required access privileges. This error is most typically caused when you write to a shared resource with read-only access.
            case DXGI_ERROR_NAME_ALREADY_EXISTS:           return "DXGI_ERROR_NAME_ALREADY_EXISTS";           // The supplied name of a resource in a call to IDXGIResource1::CreateSharedHandle is already associated with some other resource.
            case DXGI_ERROR_SDK_COMPONENT_MISSING:         return "DXGI_ERROR_SDK_COMPONENT_MISSING";         // The application requested an operation that depends on an SDK component that is missing or mismatched.
            case DXGI_ERROR_NOT_CURRENT:                   return "DXGI_ERROR_NOT_CURRENT";                   // The DXGI objects that the application has created are no longer current & need to be recreated for this operation to be performed.
            case DXGI_ERROR_HW_PROTECTION_OUTOFMEMORY:     return "DXGI_ERROR_HW_PROTECTION_OUTOFMEMORY";     // Insufficient HW protected memory exits for proper function.
            case DXGI_ERROR_DYNAMIC_CODE_POLICY_VIOLATION: return "DXGI_ERROR_DYNAMIC_CODE_POLICY_VIOLATION"; // Creating this device would violate the process's dynamic code policy.
            case DXGI_ERROR_NON_COMPOSITED_UI:             return "DXGI_ERROR_NON_COMPOSITED_UI";             // The operation failed because the compositor is not in control of the output.
            case DXGI_DDI_ERR_UNSUPPORTED:                 return "DXGI_DDI_ERR_UNSUPPORTED";
            case E_OUTOFMEMORY:                            return "E_OUTOFMEMORY";
            case E_INVALIDARG:                             return "E_INVALIDARG";                             // One or more arguments are invalid.
        }

        // return (std::string("Unknown error code: %d", std::system_category().message(error_code).c_str()).c_str());
        return "Unknown error code";
    }

	constexpr bool ErrorCheck(const HRESULT result)
	{
		if (FAILED(result))
		{
			printf("[RHI::DX12] %s\n", HResultToString(result));
			return false;
		}

		return true;
	}

    template<class T> void SafeRelease(T& ptr)
    {
        if (ptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    }
}

namespace Styx::RHI
{
    IDXGIFactory7* m_dxgi_factory = nullptr;
    ID3D12Device9* m_device = nullptr;

	bool DX12::Initialize(void* sdl_window, int32_t adpater_index)
	{
#if defined(DEBUG)
        ID3D12Debug* debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
        {
            debug_controller->EnableDebugLayer();
            SafeRelease(debug_controller);
        }
#endif

        if (!ErrorCheck(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgi_factory))))
        {
			return false;
        }

        // NOTE: Picking the GPU with the most amount of VRAM
        IDXGIAdapter1* adapter = nullptr;
        uint32_t best_adapter_index = 0;
        size_t best_adapter_memory = 0;

        for (uint32_t adapter_index = 0; m_dxgi_factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND; adapter_index++)
        {
            DXGI_ADAPTER_DESC1 adapter_desc;
            if (!ErrorCheck(adapter->GetDesc1(&adapter_desc)))
            {
                return false;
            }

            if (adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

            if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, _uuidof(ID3D12Device), nullptr))) continue;

            if (adapter_desc.DedicatedVideoMemory > best_adapter_memory)
            {
                best_adapter_memory = adapter_desc.DedicatedVideoMemory;
                best_adapter_index = adapter_index;
            }

            SafeRelease(adapter);
        }

        if (best_adapter_memory == 0)
        {
			printf("[RHI::DX12] Failed to find a suitable GPU.\n");
            return false;
        }

        m_dxgi_factory->EnumAdapters1(best_adapter_index, &adapter);

		DXGI_ADAPTER_DESC1 adapter_desc;
		adapter->GetDesc1(&adapter_desc);
        wprintf(L"[RHI::DX12] Selected GPU: %s\n", adapter_desc.Description);

        if (!ErrorCheck(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device))))
        {
            return false;
        }

        return true;
	}

	void DX12::Teardown()
	{
		SafeRelease(m_device);
		SafeRelease(m_dxgi_factory);

#if defined(DEBUG)
        IDXGIDebug1* debug = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
        {
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            SafeRelease(debug);
        }
#endif
	}

	void DX12::Tick()
	{
	}
}
