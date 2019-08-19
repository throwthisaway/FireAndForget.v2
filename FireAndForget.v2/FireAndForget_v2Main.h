#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Renderer.h"
#include "..\source\cpp\Scene.hpp"

// Renders Direct3D content on the screen.
namespace FireAndForget_v2
{
	class FireAndForget_v2Main
	{
	public:
		FireAndForget_v2Main();
		void CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void Update();
		bool Render();

		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();

		void PointerMoved(float x, float y, bool left, bool middle, bool right, size_t modifiers);
		void PointerPressed(float x, float y, bool left, bool middle, bool right);
		void PointerReleased(float x, float y, bool left, bool middle, bool right);
		void Zoom(float factor);
		void KeyDown(Windows::System::VirtualKey);
		void KeyUp(Windows::System::VirtualKey);

	private:
		// TODO: Replace with your own content renderers.
		std::unique_ptr<Renderer> m_sceneRenderer;
		// Rendering loop timer.
		DX::StepTimer m_timer;
		Scene scene_;
	};
}