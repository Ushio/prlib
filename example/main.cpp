#include "pr.hpp"
#include <iostream>
#include <memory>

enum DemoMode {
    DemoMode_Point,
    DemoMode_Line,
    DemoMode_Text,
    DemoMode_Rays,
    DemoMode_Manip,
    DemoMode_Benchmark,
};
const char* DemoModes[] = { 
    "DemoMode_Point",
    "DemoMode_Line",
    "DemoMode_Text",
    "DemoMode_Rays",
    "DemoMode_Manip",
    "DemoMode_Benchmark",
};

class IDemo {
public:
    ~IDemo() {}
    virtual void OnDraw() = 0;
    virtual void OnImGui() = 0;
    virtual pr::ITexture *GetBackground() { return nullptr; };

    pr::Camera3D camera;
};

struct PointDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        Xoshiro128StarStar random;
        for (int i = 0; i < 1024; ++i) {
            glm::vec3 p = GenerateUniformOnSphere(random.uniformf(), random.uniformf());
            DrawPoint(p, { 255 * color.r, 255 * color.g, 255 * color.b }, size);
        }
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Point")) {
            ImGui::SliderFloat("size", &size, 0, 32);
            ImGui::ColorPicker3("color", glm::value_ptr(color));
            ImGui::TreePop();
        }
    }
    glm::vec3 color = { 0, 1, 0 };
    float size = 5;
};

struct LineDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        Xoshiro128StarStar random;
        for (int i = 0; i < 64; ++i) {
            glm::vec3 p0 = GenerateUniformOnSphere(random.uniformf(), random.uniformf());
            glm::vec3 p1 = GenerateUniformOnSphere(random.uniformf(), random.uniformf());
            DrawLine(p0, p1, { 255 * color.r, 255 * color.g, 255 * color.b }, size);
        }
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Line")) {
            ImGui::SliderFloat("width", &size, 0, 32);
            ImGui::ColorPicker3("color", glm::value_ptr(color));
            ImGui::TreePop();
        }
    }
    glm::vec3 color = { 0, 1, 0 };
    float size = 5;
};

struct TextDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        DrawText(
            {}, text, size,
            { 255 * color.r, 255 * color.g, 255 * color.b },
            outlineWidth,
            { 255 * outlineColor.r, 255 * outlineColor.g, 255 * outlineColor.b });
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Line")) {
            ImGui::InputText("text", text, sizeof(text));
            ImGui::SliderFloat("font size", &size, 0, 256);
            ImGui::ColorPicker3("color", glm::value_ptr(color));
            ImGui::SliderFloat("outline width", &outlineWidth, 0, 32);
            ImGui::ColorPicker3("outline color", glm::value_ptr(outlineColor));
            ImGui::TreePop();
        }
        
    }
    char text[128] = "hello world";
    float size = 46;
    glm::vec3 color = { 0, 0.5f, 0 };
    float outlineWidth = 2;
    glm::vec3 outlineColor = { 1, 1, 1 };
};

/*
    x  : intersected t. -1 is no-intersected
    yzw: un-normalized normal
*/
glm::vec4 intersect_sphere(glm::vec3 ro, glm::vec3 rd, glm::vec3 o, float r) {
    float A = glm::dot(rd, rd);
    glm::vec3 S = ro - o;
    glm::vec3 SxRD = cross(S, rd);
    float D = A * r * r - glm::dot(SxRD, SxRD);

    if (D < 0.0f) {
        return glm::vec4(-1);
    }

    float B = glm::dot(S, rd);
    float sqrt_d = sqrt(D);
    float t0 = (-B - sqrt_d) / A;
    if (0.0f < t0) {
        glm::vec3 n = (rd * t0 + S);
        return glm::vec4(t0, n);
    }

    float t1 = (-B + sqrt_d) / A;
    if (0.0f < t1) {
        glm::vec3 n = (rd * t1 + S);
        return glm::vec4(t1, n);
    }
    return glm::vec4(-1);
}
glm::vec4 combine(glm::vec4 a, glm::vec4 b) {
    if (a.x < 0.0f) {
        return b;
    }
    if (b.x < 0.0f) {
        return a;
    }
    if (a.x < b.x) {
        return a;
    }
    return b;
}

struct RaysDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        if (_showWire) {
            DrawSphere({ -2, 0, 0 }, 0.5f, { 255, 255, 255 }, 16, 16);
            DrawSphere({ 0, 0, 0 }, 1.0f, { 255, 255, 255 }, 16, 16);
            DrawSphere({ 4, 0, 0 }, 2.0f, { 255, 255, 255 }, 16, 16);
        }

        // ray trace
        glm::mat4 vp = GetCurrentProjMatrix() * GetCurrentViewMatrix();
        glm::mat4 inverse_vp = glm::inverse(vp);

        Image2DRGBA8 image;
        image.allocate(GetScreenWidth() / _stride, GetScreenHeight() / _stride);

        LinearTransform i2x(0, (float)image.width(),  -1, 1);
        LinearTransform j2y(0, (float)image.height(), 1, -1);

        for (int j = 0; j < image.height(); ++j)
        {
            for (int i = 0; i < image.width(); ++i)
            {
                auto h = [](glm::vec4 v) {
                    return glm::vec3(v / v.w);
                };
                auto ro = h(inverse_vp * glm::vec4(i2x((float)i), j2y((float)j), -1 /*near*/, 1));
                auto rd = h(inverse_vp * glm::vec4(i2x((float)i), j2y((float)j), +1 /*far */, 1)) - ro;
                rd = glm::normalize(rd);

                auto isect = glm::vec4(-1);
                isect = combine(isect, intersect_sphere(ro, rd, { -2, 0, 0 }, 0.5f));
                isect = combine(isect, intersect_sphere(ro, rd, { 0, 0, 0 }, 1.0f));
                isect = combine(isect, intersect_sphere(ro, rd, { 4, 0, 0 }, 2.0f));

                if (0.0f < isect.x) {
                    glm::vec3 n(isect.y, isect.z, isect.w);
                    n = glm::normalize(n);

                    glm::vec3 color = (n + glm::vec3(1.0f)) * 0.5f;
                    image(i, j) = { 255 * color.r, 255 * color.g, 255 * color.b, 255 };
                }
                else {
                    image(i, j) = { 0, 0, 0, 255 };
                }
            }
        }
        if (_texture == nullptr) {
            _texture = CreateTexture();
        }
        _texture->upload(image);
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Rays")) {
            ImGui::Checkbox("show wire", &_showWire);
            ImGui::SliderInt("stride", &_stride, 1, 8);

            if (_texture) {
                ImGui::Image(_texture, ImVec2((float)_texture->width(), (float)_texture->height()));
            }
            ImGui::TreePop();
        }
    }
    pr::ITexture *GetBackground() {
        return _texture;
    }
private:
    pr::ITexture *_texture = nullptr;
    bool _showWire = true;
    int _stride = 4;
};

struct ManipDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;
        ManipulatePosition(camera, &_position0, _size);
        ManipulatePosition(camera, &_position1, _size);
        DrawSphere(_position0, 0.5f, { 255, 128, 128 });
        DrawSphere(_position1, 0.5f, { 128, 128, 255 });
        DrawTube(_position0, _position1, 0.5f, 0.5f, { 128, 128, 128 }, 32);
    }
    void OnImGui() override {
        using namespace pr;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Manip")) {
            ImGui::SliderFloat("size", &_size, 0, 3.0f);
            ImGui::TreePop();
        }
    }
    glm::vec3 _position0 = { -1, 0, 0 };
    glm::vec3 _position1 = { +1, 0, 0 };

    float _size = 0.3f;
};

struct BenchmarkDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        Xoshiro128StarStar random;
        float size = 10.0f;
        for (int i = 0; i < N; ++i) {
            glm::vec3 o = { 
                glm::mix(-size, size, random.uniformf()), 
                glm::mix(-size, size, random.uniformf()), 
                glm::mix(-size, size, random.uniformf()) 
            };
            glm::vec3 d = GenerateUniformOnSphere(random.uniformf(), random.uniformf());

            glm::u8vec3 color = {
                random.uniformi() % 256,
                random.uniformi() % 256,
                random.uniformi() % 256
            };
            switch (random.uniformi() % 4) {
                case 0:
                    DrawCircle(o, d, color, 0.1f);
                    break;
                case 1:
                    DrawTube(o, o + d * 0.2f, 0.1f, 0.1f, { color });
                    break;
                case 2:
                    DrawSphere(o, 0.1f, { color });
                    break;
                case 3:
                    DrawArrow(o, o + d * 0.2f, 0.01f, { color });
            }
        }
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Benchmark")) {
            ImGui::SliderInt("N", &N, 0, 100000);
            ImGui::TreePop();
        }
    }
    float size = 5;
    int N = 32000;
};

std::vector<IDemo *> demos = {
    new PointDemo(),
    new LineDemo(),
    new TextDemo(),
    new RaysDemo(),
    new ManipDemo(),
    new BenchmarkDemo()
};

int main() {
    using namespace pr;

    SetDataDir(JoinPath(ExecutableDir(), "../data"));

    Config config;
    config.ScreenWidth = 1920;
    config.ScreenHeight = 1080;
    config.SwapInterval = 1;
    Initialize(config);

    Camera3D camera;
    camera.origin = { 4, 4, 4 };
    camera.lookat = { 0, 0, 0 };
    camera.zNear = 0.1f;
    camera.zUp = false;

    double e = GetElapsedTime();

    bool showImGuiDemo = false;
    float fontSize = 16.0f;
    int demoMode = DemoMode_Point;

    while (pr::NextFrame() == false) {
        if (IsImGuiUsingMouse() == false) {
            UpdateCameraBlenderLike(&camera);
        }
        demos[demoMode]->camera = camera;

        if (ITexture *bg = demos[demoMode]->GetBackground()) {
            ClearBackground(bg);
        }
        else {
            ClearBackground(0.1f, 0.1f, 0.1f, 1);
        }

        BeginCamera(camera);
        PushGraphicState();

        DrawGrid(GridAxis::XZ, 1.0f, 10, { 128, 128, 128 });
        DrawXYZAxis(1.0f);

        demos[demoMode]->OnDraw();
        
        PopGraphicState();
        EndCamera();

        // DrawTextScreen(20, 20, R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)");

        BeginImGui();

        ImGui::SetNextWindowSize({ 500, 800 }, ImGuiCond_Once);
        ImGui::Begin("Panel");
        ImGui::Text("fps = %f", GetFrameRate());
        ImGui::Combo("Demo Mode", &demoMode, DemoModes, IM_ARRAYSIZE(DemoModes));
        ImGui::Checkbox("show ImGui Demo", &showImGuiDemo);
        
        demos[demoMode]->OnImGui();

        ImGui::End();

        if (showImGuiDemo) {
            ImGui::ShowDemoWindow();
        }
        EndImGui();
    }

    pr::CleanUp();
}
