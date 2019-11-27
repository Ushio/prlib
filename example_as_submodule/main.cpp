#include "pr.hpp"
#include <iostream>
#include <memory>

int main() {
    using namespace pr;

    Config config;
    config.ScreenWidth = 1920;
    config.ScreenHeight = 1080;
    config.SwapInterval = 1;
    Initialize(config);

    Camera3D camera;
    camera.origin = { 4, 4, 4 };
    camera.lookat = { 0, 0, 0 };
    camera.zUp = true;

    double e = GetElapsedTime();

    while (pr::NextFrame() == false) {
        if (IsImGuiUsingMouse() == false) {
            UpdateCameraBlenderLike(&camera);
        }

        ClearBackground(0.1f, 0.1f, 0.1f, 1);

        BeginCamera(camera);

        PushGraphicState();

        DrawGrid(GridAxis::XY, 1.0f, 10, { 128, 128, 128 });
        DrawXYZAxis(1.0f);

        static float eta_i = 1.0f;
        static float eta_o = 1.5f;
        static glm::vec3 wi = { 0, 0, 1 };
        static glm::vec3 wo = { 0, 0, 1 };
        DrawArrow(glm::vec3(), wi, 0.01f, { 255, 255, 255 });
        DrawArrow(glm::vec3(), wo, 0.01f, { 255, 255, 255 });

        glm::vec3 ht = -wi * eta_i - wo * eta_o;
        DrawArrow(glm::vec3(), ht, 0.01f, { 0, 255, 255 });

        PopGraphicState();
        EndCamera();

        BeginImGui();

        ImGui::SetNextWindowSize({ 500, 800 }, ImGuiCond_Once);
        ImGui::Begin("Panel");
        ImGui::Text("fps = %f", GetFrameRate());

        ImGui::SliderFloat("eta i", &eta_i, 0.5f, 1.5f);
        ImGui::SliderFloat("eta o", &eta_o, 0.5f, 1.5f);
        SliderDirection("wi", &wi);
        SliderDirection("wo", &wo);

        ImGui::End();

        EndImGui();
    }

    CleanUp();
}
