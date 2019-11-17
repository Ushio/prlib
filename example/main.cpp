#include "pr.hpp"
#include <iostream>

int main() {
    using namespace pr;

    Config config;
    config.ScreenWidth = 1920;
    config.ScreenHeight = 1080;
    config.SwapInterval = 4;
    Initialize(config);

    Camera3D camera;
    camera.origin = { 4, 4, 4 };
    camera.lookat = { 0, 0, 0 };

    while (pr::ProcessSystem() == false) {
        //printf("p=%s, down=%s, up=%s\n", 
        //    IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) ? "_" : "^",
        //    IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ? "o" : " ",
        //    IsMouseButtonUp(MOUSE_BUTTON_MIDDLE) ? "o" : " ");

        UpdateCameraBlenderLike(&camera);
        SetDepthTest(true);

        ClearBackground(0, 0, 0, 1);
        BeginCamera(camera);

        DrawGrid(GridAxis::XZ, 1.0f, 10, { 255, 255, 255 });

        DrawLine({}, { 1, 0, 0 }, { 255, 0, 0 });
        DrawLine({}, { 0, 1, 0 }, { 0, 255, 0 });
        DrawLine({}, { 0, 0, 1 }, { 0, 0, 255 });

        DrawCircle({}, { 255, 0, 0 }, 0.5f);

        static Primitive prim;
        static IRandom *random = CreateRandomNumberGenerator(100);
        for (int i = 0; i < 0; ++i) {
            glm::u8vec3 color = glm::u8vec3(
                (random->uniform(0, 256)),
                (random->uniform(0, 256)),
                (random->uniform(0, 256))
            );

            // DrawLine(
            //     { random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f },
            //     { random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f },
            //     color
            // );
            // DrawPoint({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, random->uniform(0, 10));
           // DrawPoint({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, 1);
            DrawCircle({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, 0.01f, 10, 5);

            // prim.add({ random.uniform(-0.9f, 0.9f), random.uniform(-0.9f, 0.9f), 0 }, color);
            // prim.add({ random.uniform(-0.9f, 0.9f), random.uniform(-0.9f, 0.9f), 0 }, color);
        }

        EndCamera();

        // prim.draw(PrimitiveMode::Lines);
        prim.clear();
    }

    CleanUp();
}
