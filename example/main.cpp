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
    camera.zUp = false;

    //Image2DRGBA8 image;
    //image.allocate(2, 2);
    //image(0, 0) = glm::u8vec4(255);
    //image(1, 0) = glm::u8vec4(255);
    //image(0, 1) = glm::u8vec4(255);
    //image(1, 1) = glm::u8vec4(255);

    //ITextureRGBA8 *texture = CreateTextureRGBA8();
    //texture->upload(image);

    double e = GetElapsedTime();

    while (pr::ProcessSystem() == false) {
        if (IsImGuiUsingMouse() == false) {
            UpdateCameraBlenderLike(&camera);
        }

        ClearBackground(0.1f, 0.1f, 0.1f, 1);

        BeginCamera(camera);

        PushGraphicState();
        SetDepthTest(false);
        SetBlendMode(BlendMode::Alpha);

        // xy Z order
        Image2DRGBA8 image;
        image.allocate(2, 2);
        image(0, 0) = glm::u8vec4(255, 0, 0, 128);
        image(1, 0) = glm::u8vec4(0, 255, 0, 128);
        image(0, 1) = glm::u8vec4(0, 0, 255, 128);
        image(1, 1) = glm::u8vec4(255, 255, 255, 128);

        std::unique_ptr<ITextureRGBA8> texture(CreateTextureRGBA8());
        texture->upload(image);

        DrawGrid(GridAxis::XY, 1.0f, 10, { 128, 128, 128 });
        DrawXYZAxis(1.0f);

        TriBegin(texture.get());
        uint32_t vs[4];
        vs[0] = TriVertex({ -1.000000, 1.000000, 0.000000 }, { 0.000000, 1.000000 }, { 255, 255, 255, 255 });
        vs[1] = TriVertex({ 1.000000, 1.000000, 0.000000 }, { 1.000000, 1.000000 }, { 255, 255, 255, 255 });
        vs[2] = TriVertex({ -1.000000, -1.000000, 0.000000 }, { 0.000000, 0.000000 }, { 255, 255, 255, 255 });
        vs[3] = TriVertex({ 1.000000, -1.000000, 0.000000 }, { 1.000000, 0.000000 }, { 255, 255, 255, 255 });
        TriIndex(vs[0]); TriIndex(vs[1]); TriIndex(vs[2]);
        TriIndex(vs[1]); TriIndex(vs[2]); TriIndex(vs[3]);
        TriEnd();

        DrawCircle({100, 100, 0}, { 255, 0, 0 }, 50.0f);

        // DrawTube({}, { 0, 1, 1 }, 0.3f, 0.05f, { 0, 255, 255 });
        // DrawArrow({}, { 1, 0, 1 }, 0.01f, { 0, 255, 255 });

        //DrawCircle({}, { 255, 0, 0 }, 0.5f);
        //DrawTube({}, { 0, 1, 0 }, 1.0f, 1.0f, { 128, 128, 128 });

        //LinearTransform<float> xmap(0, 31, -2, 2);
        //LinearTransform<float> ymap(0, 31, -2, 2);
        //for (int x = 0; x < 32; ++x) {
        //    for (int y = 0; y < 32; ++y) {
        //        float cx = xmap((float)x);
        //        float cy = ymap((float)y);

        //        auto d = glm::normalize(glm::vec3(-cy, cx, 0));
        //        DrawArrow(glm::vec3(cx, cy, 0), glm::vec3(cx, cy, 0) + d * 0.1f, 0.005f, { 255, 255, 255 });
        //    }
        //}
        // PrimBegin(PrimitiveMode::Points, 1);

        Xoshiro128StarStar random;

        for (int i = 0; i < 10000; ++i) {
            float x = glm::mix(-4.0f, 4.0f, random.uniformf());
            float y = glm::mix(-4.0f, 4.0f, random.uniformf());
            float z = glm::mix(-4.0f, 4.0f, random.uniformf());

            float nx = glm::mix(-1.0f, 1.0f, random.uniformf());
            float ny = glm::mix(-1.0f, 1.0f, random.uniformf());
            float nz = glm::mix(-1.0f, 1.0f, random.uniformf());

            // PrimVertex({ x, y, z }, { 255, 255, 255 });

            // DrawArrow(glm::vec3(x,y,z), glm::vec3(x, y, z) + glm::vec3(nx, ny, nz) * 0.1f, 0.005f, { 255, 255, 255 }, 8);

           // glm::u8vec3 color = glm::u8vec3(
           //     (random->uniform(0, 256)),
           //     (random->uniform(0, 256)),
           //     (random->uniform(0, 256))
           // );
        }

        PopGraphicState();
        EndCamera();

        BeginImGui();

        ImGui::SetNextWindowSize({ 500, 800 }, ImGuiCond_Once);
        ImGui::Begin("Panel");
        ImGui::Text("fps = %f", GetFrameRate());
        ImGui::End();

        ImGui::ShowDemoWindow();
        EndImGui();
    }

    CleanUp();
}
