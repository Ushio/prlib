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

    double e = GetElapsedTime();

    while (pr::ProcessSystem() == false) {
        double deltaTime = GetElapsedTime() - e;
        e = GetElapsedTime();
        printf("fps : %f\n", 1.0 / deltaTime);
        //printf("p=%s, down=%s, up=%s\n", 
        //    IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) ? "_" : "^",
        //    IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ? "o" : " ",
        //    IsMouseButtonUp(MOUSE_BUTTON_MIDDLE) ? "o" : " ");

        UpdateCameraBlenderLike(&camera);
        SetDepthTest(true);

        ClearBackground(0.1f, 0.1f, 0.1f, 1);
        BeginCamera(camera);

        DrawGrid(GridAxis::XY, 1.0f, 10, { 128, 128, 128 });
        DrawXYZAxis(1.0f);

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

        std::unique_ptr<IRandom> random( CreateRandomNumberGenerator(100) );
        for (int i = 0; i < 10000; ++i) {
            float x = random->uniform(-4.0f, 4.0f);
            float y = random->uniform(-4.0f, 4.0f);
            float z = random->uniform(-4.0f, 4.0f);

            float nx = random->uniform(-1.0f, 1.0f);
            float ny = random->uniform(-1.0f, 1.0f);
            float nz = random->uniform(-1.0f, 1.0f);

            // PrimVertex({ x, y, z }, { 255, 255, 255 });

            DrawArrow(glm::vec3(x,y,z), glm::vec3(x, y, z) + glm::vec3(nx, ny, nz) * 0.1f, 0.005f, { 255, 255, 255 }, 8);

           // glm::u8vec3 color = glm::u8vec3(
           //     (random->uniform(0, 256)),
           //     (random->uniform(0, 256)),
           //     (random->uniform(0, 256))
           // );

             //DrawLine(
             //    { random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f },
             //    { random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f },
             //    { 255, 255, 255 }
             //);
           // // DrawPoint({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, random->uniform(0, 10));
           //// DrawPoint({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, 1);
           // DrawCircle({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, 0.01f, 10, 5);

           // // prim.add({ random.uniform(-0.9f, 0.9f), random.uniform(-0.9f, 0.9f), 0 }, color);
           // // prim.add({ random.uniform(-0.9f, 0.9f), random.uniform(-0.9f, 0.9f), 0 }, color);
        }
        // PrimEnd();

        EndCamera();
    }

    CleanUp();
}
