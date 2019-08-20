#pragma once
namespace UI {
	bool Init(ID3D12Device* device, IDXGISwapChain* swapchain);
	void Update(double frame, double total);
	void BeforeResize();
	void OnResize(ID3D12Device* device, IDXGISwapChain* swapchain, int width, int height);
	ID3D12GraphicsCommandList* Render(int index);
	void Shutdown();

	void UpdateMousePos(int x, int y);
	void UpdateMouseButton(bool l, bool r, bool m);
	void UpdateMouseWheel();
	void UpdateKeyboard(int key, bool down);
	void UpdateKeyboardInput(int key);
	void UpdateKeyboardModifiers(bool ctrl, bool alt, bool shift);
	bool UpdateMouseCursor(Windows::UI::Core::CoreWindow^ window);
}