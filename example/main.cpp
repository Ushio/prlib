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
    camera.lookat = { 0, 1, 0 };
    camera.zNear = 0.0001f;
    camera.zUp = true;

    //Image2DRGBA8 image;
    //image.allocate(2, 2);
    //image(0, 0) = glm::u8vec4(255);
    //image(1, 0) = glm::u8vec4(255);
    //image(0, 1) = glm::u8vec4(255);
    //image(1, 1) = glm::u8vec4(255);

    //ITextureRGBA8 *texture = CreateTextureRGBA8();
    //texture->upload(image);

    double e = GetElapsedTime();

    while (pr::NextFrame() == false) {
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

        // Rectangle
        //TriBegin(texture.get());
        //uint32_t vs[4];
        //vs[0] = TriVertex({ -1.000000, 1.000000, 0.000000 }, { 0.000000, 1.000000 }, { 255, 255, 255, 255 });
        //vs[1] = TriVertex({ 1.000000, 1.000000, 0.000000 }, { 1.000000, 1.000000 }, { 255, 255, 255, 255 });
        //vs[2] = TriVertex({ -1.000000, -1.000000, 0.000000 }, { 0.000000, 0.000000 }, { 255, 255, 255, 255 });
        //vs[3] = TriVertex({ 1.000000, -1.000000, 0.000000 }, { 1.000000, 0.000000 }, { 255, 255, 255, 255 });
        //TriIndex(vs[0]); TriIndex(vs[1]); TriIndex(vs[2]);
        //TriIndex(vs[1]); TriIndex(vs[2]); TriIndex(vs[3]);
        //TriEnd();

        Xoshiro128StarStar random;

        static glm::vec3 dir = {0, 0, 1};
        DrawLine(glm::vec3(), dir, { 255, 255, 255 });

        glm::vec3 u, v;
        GetOrthonormalBasis(dir, &u, &v);

        float cosTheta = 1.0f - FLT_EPSILON * 5;
        double tanTheta = 0.75 * std::sqrt(1.0 - (double)cosTheta * (double)cosTheta) / (double)cosTheta;

        PrimBegin(PrimitiveMode::Points);
        for (int i = 0; i < 500; ++i) {
            // glm::vec3 p = GenerateUniformOnSphereLimitedAngle(random.uniformf(), random.uniformf(), cosTheta);
            
            glm::vec2 circle = GenerateUniformInCircle(random.uniformf(), random.uniformf());
            glm::vec3 p = glm::vec3(tanTheta * circle.x, tanTheta * circle.y, 1.0f);
            glm::vec3 sp = glm::normalize(u * p.x + v * p.y + dir * p.z);
            PrimVertex(sp, { 255, 255, 255 });
        }
        PrimEnd();

        double r = std::sqrt(1.0 - (double)cosTheta * (double)cosTheta) / (double)cosTheta;
        DrawCircle(dir, dir, { 255, 0, 0 }, r);

        PopGraphicState();
        EndCamera();

        BeginImGui();

        ImGui::SetNextWindowSize({ 500, 800 }, ImGuiCond_Once);
        ImGui::Begin("Panel");
        ImGui::Text("fps = %f", GetFrameRate());

        SliderDirection("dir", &dir);

        ImGui::End();

        ImGui::ShowDemoWindow();
        EndImGui();
    }

    CleanUp();
}
