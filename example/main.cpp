#include "pr.hpp"
#include <iostream>

int main() {
    pr::Config config;
    config.ScreenWidth = 1920;
    config.ScreenHeight = 1080;
    pr::Initialize(config);

    while (pr::ProcessSystem() == false) {
        pr::ClearBackground(0, 0, 0, 1);
        


    }

    pr::CleanUp();
}
