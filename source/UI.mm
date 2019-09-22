#include "UI.h"
#include "imgui.h"
#include "imgui_impl_metal.h"


namespace ui {
	namespace {
		bool show_demo_window = true, show_another_window = true;
	}
bool Init(id<MTLDevice> device) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
	//io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

	//io.ImeWindowHandle = hwnd;

	// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array that we will update during the application lifetime.
//	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
//	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
//	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
//	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
//	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
//	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
//	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
//	io.KeyMap[ImGuiKey_Home] = VK_HOME;
//	io.KeyMap[ImGuiKey_End] = VK_END;
//	io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
//	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
//	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
//	io.KeyMap[ImGuiKey_Space] = VK_SPACE;
//	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
//	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
//	io.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';

	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	io.Fonts->AddFontDefault();
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();
	io.BackendPlatformName = "imgui_impl_metal";
	ImGui_ImplMetal_Init(device);
	return true;
}
id<MTLCommandBuffer> Render(id<MTLTexture> tex, id<MTLCommandBuffer> commandBuffer) {
	ImGuiIO &io = ImGui::GetIO();
	io.DisplaySize.x = tex.width;
	io.DisplaySize.y = tex.height;
	io.DisplayFramebufferScale = ImVec2(1, 1);

	//io.DeltaTime = 1 / float(view.preferredFramesPerSecond ?: 60);
	io.DeltaTime = 1.f / 60.f;

	static bool show_demo_window = true;
	static bool show_another_window = false;
	static float clear_color[4] = { 0.28f, 0.36f, 0.5f, 1.0f };

	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = tex;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].level = 0;
	if (passDescriptor != nil)
	{
		//passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);

		ImGui_ImplMetal_NewFrame(passDescriptor);
		// Here, you could do additional rendering work, including other passes as necessary.

		id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
		[renderEncoder pushDebugGroup:@"ImGui demo"];

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData *drawData = ImGui::GetDrawData();
        ImGui_ImplMetal_RenderDrawData(drawData, commandBuffer, renderEncoder);

        [renderEncoder popDebugGroup];
        [renderEncoder endEncoding];
	}
	return commandBuffer;
}
void Shutdown() {
	ImGui_ImplMetal_DestroyDeviceObjects();
}
void Update(double frame, double total) {
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = frame / 1000.;
	ImGui::NewFrame();
}
void OnResize(int width, int height) {
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = width; io.DisplaySize.y = height;
}
}
