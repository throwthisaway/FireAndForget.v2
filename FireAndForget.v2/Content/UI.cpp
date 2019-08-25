#include "pch.h"
#include "UI.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include "imgui.h"
#include "imgui_impl_dx12.h"
//#include <d3d11.h>
//#include <d3d11on12.h>
//#include "imgui_impl_dx11.h"

namespace UI {
	using namespace Microsoft::WRL;
	namespace {
		// Data
		static int const                    NUM_FRAMES_IN_FLIGHT = 3;
		static ID3D12CommandAllocator* g_pCommandAllocators[NUM_FRAMES_IN_FLIGHT] = {};
		static UINT                         g_frameIndex = 0;

		static int const                    NUM_BACK_BUFFERS = 3;
		static ID3D12DescriptorHeap* g_pd3dRtvDescHeap = NULL;
		static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
		static ID3D12GraphicsCommandList* g_pd3dCommandList = NULL;
		static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};
		ImGuiMouseCursor g_LastMouseCursor = ImGuiMouseCursor_Arrow;
		bool show_demo_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	}
	void CreateRTVs(ID3D12Device* device, IDXGISwapChain* swapchain) {
		for (UINT i = 0; i < NUM_BACK_BUFFERS; ++i) {
			ComPtr<ID3D12Resource> pBackBuffer;
			swapchain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
			device->CreateRenderTargetView(pBackBuffer.Get(), NULL, g_mainRenderTargetDescriptor[i]);
		}
	}
	bool Init(ID3D12Device* device,	IDXGISwapChain* swapchain) {
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = NUM_BACK_BUFFERS;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 1;
			if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
				return false;

			SIZE_T rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
			{
				g_mainRenderTargetDescriptor[i] = rtvHandle;
				rtvHandle.ptr += rtvDescriptorSize;
			}

			CreateRTVs(device, swapchain);
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
				return false;
		}

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
			if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_pCommandAllocators[i])) != S_OK)
				return false;

		if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_pCommandAllocators[0], NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
			g_pd3dCommandList->Close() != S_OK)
			return false;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
		//io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
		io.BackendPlatformName = "imgui_impl_uwp";
		//io.ImeWindowHandle = hwnd;

		// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array that we will update during the application lifetime.
		io.KeyMap[ImGuiKey_Tab] = VK_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
		io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
		io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
		io.KeyMap[ImGuiKey_Home] = VK_HOME;
		io.KeyMap[ImGuiKey_End] = VK_END;
		io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
		io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
		io.KeyMap[ImGuiKey_Space] = VK_SPACE;
		io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
		io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
		io.KeyMap[ImGuiKey_A] = 'A';
		io.KeyMap[ImGuiKey_C] = 'C';
		io.KeyMap[ImGuiKey_V] = 'V';
		io.KeyMap[ImGuiKey_X] = 'X';
		io.KeyMap[ImGuiKey_Y] = 'Y';
		io.KeyMap[ImGuiKey_Z] = 'Z';

		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		// TODO::ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(device, NUM_FRAMES_IN_FLIGHT,
			DXGI_FORMAT_B8G8R8A8_UNORM, // TODO:: from deviceresources
			g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
		io.Fonts->AddFontDefault();
		return true;
	}
	void BeforeResize() {
		ImGui_ImplDX12_InvalidateDeviceObjects();
	}
	void OnResize(ID3D12Device* device, IDXGISwapChain* swapchain, int width, int height) {
		//if (!ImGui::GetCurrentContext()) return;
		CreateRTVs(device, swapchain);
		ImGui_ImplDX12_CreateDeviceObjects();
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize.x = width; io.DisplaySize.y = height;
	}
	void Shutdown() {
		ImGui_ImplDX12_Shutdown();
		// TODO:: ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
			if (g_pCommandAllocators[i]) { g_pCommandAllocators[i]->Release(); g_pCommandAllocators[i] = NULL; }
		if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
		if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = NULL; }
		if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }
	}
	void Update(double frame, double total) {
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = frame / 1000.;
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();

		//// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		//if (show_demo_window)
		//	ImGui::ShowDemoWindow(&show_demo_window);

		//// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		//{
		//	static float f = 0.0f;
		//	static int counter = 0;

		//	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		//	ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		//	ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		//	ImGui::Checkbox("Another Window", &show_another_window);

		//	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		//	ImGui::ColorEdit3("clear color", (float*)& clear_color); // Edit 3 floats representing a color

		//	if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
		//		counter++;
		//	ImGui::SameLine();
		//	ImGui::Text("counter = %d", counter);

		//	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		//	ImGui::End();
		//}

		//// 3. Show another simple window.
		//if (show_another_window)
		//{
		//	ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		//	ImGui::Text("Hello from another window!");
		//	if (ImGui::Button("Close Me"))
		//		show_another_window = false;
		//	ImGui::End();
		//}
	}
	ID3D12GraphicsCommandList* Render(int index) {
		g_pCommandAllocators[index]->Reset();
		g_pd3dCommandList->Reset(g_pCommandAllocators[index], NULL);
        g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[index], FALSE, NULL);
        g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
		return g_pd3dCommandList;
	}
	bool UpdateMousePos(int x, int y) {
		if (!ImGui::GetCurrentContext()) return false;
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = ImVec2((float)x, (float)y);
		return io.WantCaptureMouse;
	}
	bool UpdateMouseButton(bool l, bool r, bool m) {
		if (!ImGui::GetCurrentContext()) return false;
		ImGuiIO& io = ImGui::GetIO();
		int button = 0;
		io.MouseDown[0] = l;
		io.MouseDown[1] = r;
		io.MouseDown[2] = m;
		return io.WantCaptureMouse;
	}
	bool UpdateMouseWheel(int delta, bool horizontal) {
		if (!ImGui::GetCurrentContext()) return false;
		ImGuiIO& io = ImGui::GetIO();
		delta /= WHEEL_DELTA;
		if (horizontal) io.MouseWheelH += delta;
		else io.MouseWheel += (float)delta;
		return true;
	}
	bool UpdateKeyboard(int key, bool down) {
		if (!ImGui::GetCurrentContext()) return false;
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[key] = down;
		return io.WantCaptureKeyboard;
	}
	bool UpdateKeyboardInput(int key) {
		if (!ImGui::GetCurrentContext()) return false;
		ImGuiIO& io = ImGui::GetIO();
		io.AddInputCharacter(key);
		return io.WantCaptureKeyboard;
	}
	void UpdateKeyboardModifiers(bool ctrl, bool alt, bool shift) {
		if (!ImGui::GetCurrentContext()) return;
		ImGuiIO& io = ImGui::GetIO();
		io.KeyCtrl = ctrl;
		io.KeyShift = shift;
		io.KeyAlt = alt;
		io.KeySuper = false;
	}
	bool UpdateMouseCursor(Windows::UI::Core::CoreWindow^ window) {
		if (!ImGui::GetCurrentContext()) return false;
		ImGuiIO& io = ImGui::GetIO();
		// Update OS mouse cursor with the cursor requested by imgui
		//ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
		if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) return false;
		ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
		if (g_LastMouseCursor == imgui_cursor) {
			return false;
		}
		g_LastMouseCursor = imgui_cursor;
		if (imgui_cursor == ImGuiMouseCursor_None) {
			// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
			window->PointerCursor = nullptr;
		}
		else {
			// Show OS mouse cursor
			Windows::UI::Core::CoreCursorType cursor = Windows::UI::Core::CoreCursorType::Arrow;
			switch (imgui_cursor)
			{
			case ImGuiMouseCursor_Arrow:        cursor = Windows::UI::Core::CoreCursorType::Arrow; break;
			case ImGuiMouseCursor_TextInput:    cursor = Windows::UI::Core::CoreCursorType::IBeam; break;
			case ImGuiMouseCursor_ResizeAll:    cursor = Windows::UI::Core::CoreCursorType::SizeAll; break;
			case ImGuiMouseCursor_ResizeEW:     cursor = Windows::UI::Core::CoreCursorType::SizeWestEast; break;
			case ImGuiMouseCursor_ResizeNS:     cursor = Windows::UI::Core::CoreCursorType::SizeNorthSouth; break;
			case ImGuiMouseCursor_ResizeNESW:   cursor = Windows::UI::Core::CoreCursorType::SizeNortheastSouthwest; break;
			case ImGuiMouseCursor_ResizeNWSE:   cursor = Windows::UI::Core::CoreCursorType::SizeNorthwestSoutheast; break;
			case ImGuiMouseCursor_Hand:         cursor = Windows::UI::Core::CoreCursorType::Hand; break;
			}
			window->PointerCursor = ref new Windows::UI::Core::CoreCursor(cursor, 0);
		}
		return true;
	}
	//	namespace {
	//		ComPtr<ID3D11Device> g_pd3dDevice;
	//		ComPtr<ID3D11DeviceContext> g_pd3dDeviceContext;
	//		std::vector<ComPtr<ID3D11RenderTargetView>> g_mainRenderTargetViews;
	//		std::vector<ComPtr<ID3D11Resource>> d3d11BackBuffers;
	//		ComPtr<ID3D11On12Device> g_d3d11on12Device;
	//
	//		bool show_demo_window = true;
	//		bool show_another_window = false;
	//		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	//	}
	//	void CreateRTVs(IDXGISwapChain* swapChain) {
	//		DXGI_SWAP_CHAIN_DESC desc;
	//		swapChain->GetDesc(&desc);
	//		g_mainRenderTargetViews.resize(desc.BufferCount);
	//		d3d11BackBuffers.resize(desc.BufferCount);
	//		for (UINT i = 0; i < desc.BufferCount; ++i) {
	//			ComPtr<ID3D12Resource> backBuffer;
	//			swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
	//			D3D11_RESOURCE_FLAGS flags = {};
	//			flags.BindFlags = D3D11_BIND_RENDER_TARGET;
	//			D3D12_RESOURCE_STATES inState = D3D12_RESOURCE_STATE_RENDER_TARGET, outState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//			ComPtr<ID3D11Resource> d3d11BackBuffer;
	//			HRESULT hr = g_d3d11on12Device->CreateWrappedResource(backBuffer.Get(), &flags, inState, outState, IID_PPV_ARGS(&d3d11BackBuffer));
	//			d3d11BackBuffers[i] = d3d11BackBuffer;
	//			assert(SUCCEEDED(hr));
	//			g_pd3dDevice->CreateRenderTargetView(d3d11BackBuffer.Get(), NULL, &g_mainRenderTargetViews[i]);
	//			g_d3d11on12Device->ReleaseWrappedResources(d3d11BackBuffer.GetAddressOf(), 1);
	//		}
	//	}
	//	void Init(ID3D12Device* device, IUnknown* commandQueue) {
	//		UINT flags = 0;
	//#ifdef _DEBUG
	//		flags |= D3D11_CREATE_DEVICE_DEBUG;
	//#endif
	//		const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
	//		D3D_FEATURE_LEVEL chosenFeatureLevel;
	//		HRESULT hr = ::D3D11On12CreateDevice(device, flags, &featureLevel, 1, &commandQueue, 1, 1, &g_pd3dDevice, &g_pd3dDeviceContext, &chosenFeatureLevel);
	//		assert(SUCCEEDED(hr) && featureLevel == chosenFeatureLevel);
	//		hr = g_pd3dDevice->QueryInterface<ID3D11On12Device>(&g_d3d11on12Device);
	//		assert(SUCCEEDED(hr));
	//
	//		IMGUI_CHECKVERSION();
	//		ImGui::CreateContext();
	//		ImGuiIO& io = ImGui::GetIO(); (void)io;
	//		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//
	//		// Setup Dear ImGui style
	//		ImGui::StyleColorsDark();
	//		//ImGui::StyleColorsClassic();
	//
	//		// Setup Platform/Renderer bindings
	//		//ImGui_ImplWin32_Init(hwnd);
	//		ImGui_ImplDX11_Init(g_pd3dDevice.Get(), g_pd3dDeviceContext.Get());
	//		io.Fonts->AddFontDefault();
	//	}
	//	void ResetRTVs() {
	//		for (auto& rtv : g_mainRenderTargetViews) rtv.Reset();
	//	}
	//	void OnResize(IDXGISwapChain* swapChain, int width, int height) {
	//		ResetRTVs();
	//		CreateRTVs(swapChain);
	//		ImGuiIO& io = ImGui::GetIO();
	//		io.DisplaySize.x = width; io.DisplaySize.y = height;
	//	}
	//	void NewFrame() {
	//		ImGui_ImplDX11_NewFrame();
	//        // TODO:: ImGui_ImplWin32_NewFrame();
	//        ImGui::NewFrame();
	//		if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);
	//		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	//        {
	//            static float f = 0.0f;
	//            static int counter = 0;
	//
	//            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
	//
	//            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
	//            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
	//            ImGui::Checkbox("Another Window", &show_another_window);
	//
	//            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
	//            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
	//
	//            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
	//                counter++;
	//            ImGui::SameLine();
	//            ImGui::Text("counter = %d", counter);
	//
	//            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	//            ImGui::End();
	//        }
	//	}
	//	void Render(IDXGISwapChain* swapChain, int index) {
	//		ImGui::Render();
	//		/*ComPtr<ID3D12Resource> backBuffer;
	//		swapChain->GetBuffer(index, IID_PPV_ARGS(&backBuffer));
	//		D3D11_RESOURCE_FLAGS flags = {};
	//		flags.BindFlags = D3D11_BIND_RENDER_TARGET;
	//		D3D12_RESOURCE_STATES inState = D3D12_RESOURCE_STATE_RENDER_TARGET, outState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//		ComPtr<ID3D11Resource> d3d11BackBuffer;
	//		HRESULT hr = g_d3d11on12Device->CreateWrappedResource(backBuffer.Get(), &flags, inState, outState, IID_PPV_ARGS(&d3d11BackBuffer));
	//		assert(SUCCEEDED(hr));
	//		g_pd3dDevice->CreateRenderTargetView(d3d11BackBuffer.Get(), NULL, &g_mainRenderTargetViews[index]);*/
	//		ID3D11RenderTargetView* rtv = g_mainRenderTargetViews[index].Get();
	//		ID3D11Resource* backBuffer = d3d11BackBuffers[index].Get();
	//		g_d3d11on12Device->AcquireWrappedResources(&backBuffer, 1);
	//		g_pd3dDeviceContext->OMSetRenderTargets(1, &rtv, NULL);
	//		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	//		g_pd3dDeviceContext->Flush();
	//		g_d3d11on12Device->ReleaseWrappedResources(&backBuffer, 1);
	//	}
	//	void Shutdown() {
	//		ImGui_ImplDX11_Shutdown();
	//		// TODO:: ImGui_ImplWin32_Shutdown();
	//		ImGui::DestroyContext();
	//		ResetRTVs();
	//		g_d3d11on12Device.Reset();
	//		g_pd3dDeviceContext.Reset();
	//		g_pd3dDevice.Reset();
	//	}
}