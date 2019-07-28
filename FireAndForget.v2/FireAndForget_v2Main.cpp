#include "pch.h"
#include "FireAndForget_v2Main.h"
#include "Common\DirectXHelper.h"

using namespace FireAndForget_v2;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
FireAndForget_v2Main::FireAndForget_v2Main()
{
	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

// Creates and initializes the renderers.
void FireAndForget_v2Main::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// TODO: Replace this with your app's content initialization.
	m_sceneRenderer = std::unique_ptr<Renderer>(new Renderer(deviceResources));
	scene_.Init(m_sceneRenderer.get(), (int)deviceResources->GetOutputSize().Width, (int)deviceResources->GetOutputSize().Height);
	OnWindowSizeChanged();
}

// Updates the application state once per frame.
void FireAndForget_v2Main::Update()
{
	// Update scene objects.
	m_timer.Tick([&]()
	{
		scene_.Update(DX::StepTimer::TicksToSeconds(m_timer.GetElapsedTicks()) * 1000.,
			m_timer.GetElapsedSeconds() * 1000.);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool FireAndForget_v2Main::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	scene_.Render();
	return true;
}

// Updates application state when the window's size changes (e.g. device orientation change)
void FireAndForget_v2Main::OnWindowSizeChanged()
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Notifies the app that it is being suspended.
void FireAndForget_v2Main::OnSuspending()
{
	// TODO: Replace this with your app's suspending logic.

	// Process lifetime management may terminate suspended apps at any time, so it is
	// good practice to save any state that will allow the app to restart where it left off.

	m_sceneRenderer->SaveState();

	// If your application uses video memory allocations that are easy to re-create,
	// consider releasing that memory to make it available to other applications.
}

// Notifes the app that it is no longer suspended.
void FireAndForget_v2Main::OnResuming()
{
	// TODO: Replace this with your app's resuming logic.
}

// Notifies renderers that device resources need to be released.
void FireAndForget_v2Main::OnDeviceRemoved()
{
	// TODO: Save any necessary application or renderer state and release the renderer
	// and its resources which are no longer valid.
	m_sceneRenderer->SaveState();
	m_sceneRenderer = nullptr;
}

void FireAndForget_v2Main::PointerMoved(float x, float y, bool left, bool middle, bool right, size_t modifiers) {
	const size_t Ctrl = 1, Alt = 2, Shift = 4;
	if (left) {
		if (modifiers & Alt) scene_.input.TranslateY(y);
		else scene_.input.Rotate(x, y);
	}
	else if (right) scene_.input.TranslateXZ(x, y);
	if (modifiers & Ctrl)
		scene_.UpdateSceneTransform();
	else
		scene_.UpdateCameraTransform();
}
void FireAndForget_v2Main::PointerPressed(float x, float y, bool left, bool middle, bool right) {
	scene_.input.Start(x, y);
}
void FireAndForget_v2Main::PointerReleased(float x, float y, bool left, bool middle, bool right) {
}
void FireAndForget_v2Main::KeyDown(Windows::System::VirtualKey key) {
	switch (key) {
	case Windows::System::VirtualKey::Space:
		break;
	}
}
void FireAndForget_v2Main::KeyUp(Windows::System::VirtualKey) {

}
void FireAndForget_v2Main::Zoom(float factor) {
	
}