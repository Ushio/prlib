#include "pr.hpp"
#include <iostream>

int main() {
    using namespace pr;

    Config config;
    config.ScreenWidth = 1920;
    config.ScreenHeight = 1080;
    Initialize(config);

    while (pr::ProcessSystem() == false) {
        // pr::ClearBackground(0, 0, 0, 1);

        static Primitive prim;
        static IRandom *random = CreateRandomNumberGenerator(100);
        for (int i = 0; i < 10000; ++i) {
            byte3 color = byte3(
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
            DrawPoint({ random->uniform(-0.9f, 0.9f), random->uniform(-0.9f, 0.9f), 0.0f }, color, 1);

            // prim.add({ random.uniform(-0.9f, 0.9f), random.uniform(-0.9f, 0.9f), 0 }, color);
            // prim.add({ random.uniform(-0.9f, 0.9f), random.uniform(-0.9f, 0.9f), 0 }, color);
        }
        // prim.draw(PrimitiveMode::Lines);
        prim.clear();
    }

    pr::CleanUp();
}
