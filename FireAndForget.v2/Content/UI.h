#pragma once
namespace ui {
	bool Init(ID3D12Device* device, IDXGISwapChain* swapchain);
	void Update(double frame, double total);
	void BeforeResize();
	void OnResize(ID3D12Device* device, IDXGISwapChain* swapchain, int width, int height);
	ID3D12GraphicsCommandList* Render(int index);
	void Shutdown();

	bool UpdateMousePos(int x, int y);
	bool UpdateMouseButton(bool l, bool r, bool m);
	bool UpdateMouseWheel(int delta, bool horizontal);
	bool UpdateKeyboard(int key, bool down);
	bool UpdateKeyboardInput(int key);
	void UpdateKeyboardModifiers(bool ctrl, bool alt, bool shift);
	bool UpdateMouseCursor(Windows::UI::Core::CoreWindow^ window);
}
