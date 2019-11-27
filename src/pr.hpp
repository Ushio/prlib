#pragma once

#include <string>
#include <vector>

#ifdef GLM_VERSION
    #ifndef GLM_FORCE_CTOR_INIT
        #error "please #define GLM_FORCE_CTOR_INIT or include pr.hpp first"
    #endif
    #ifndef GLM_FORCE_PURE
        #error "please #define GLM_FORCE_PURE or include pr.hpp first"
    #endif
#else
    #ifndef GLM_FORCE_CTOR_INIT
        #define GLM_FORCE_CTOR_INIT
    #endif
    #ifndef GLM_FORCE_PURE
        #define GLM_FORCE_PURE
    #endif
#endif


#include "glm/glm.hpp"
#include "glm/ext.hpp"

#include "imgui.h"

/*
 Macros
*/
#include <intrin.h>
#define PR_ASSERT(ExpectTrue) if((ExpectTrue) == 0) { __debugbreak(); }

#include "prp.hpp"

namespace pr {
    struct Config {
        int ScreenWidth = 1280;
        int ScreenHeight = 768;
        int SwapInterval = 1;
        int NumSamples = 4;
        std::string Title;
    };
    void Initialize(Config config);
    void CleanUp();

    int GetScreenWidth();
    int GetScreenHeight();

    /*
      - Swap BackBuffer
      - Process Window Event
      - return true if the window should close
    */
    bool NextFrame();

    // Realtime clock
    double GetElapsedTime();

    // FrameBase clock
    double GetFrameTime();
    double GetFrameDeltaTime();
    double GetFrameRate();

    // Inputs
    extern int MOUSE_BUTTON_LEFT;
    extern int MOUSE_BUTTON_MIDDLE;
    extern int MOUSE_BUTTON_RIGHT;

    // query mouse button state
    bool IsMouseButtonPressed(int button); 

    // up or down event
    bool IsMouseButtonDown(int button);
    bool IsMouseButtonUp(int button);

    // Mouse behavior
    glm::vec2 GetMousePosition();
    glm::vec2 GetMouseDelta();
    glm::vec2 GetMouseScrollDelta();

    // Notes: ImGui Required Keys
    extern const int KEY_TAB;
    extern const int KEY_LEFT;
    extern const int KEY_RIGHT;
    extern const int KEY_UP;
    extern const int KEY_DOWN;
    extern const int KEY_PAGE_UP;
    extern const int KEY_PAGE_DOWN;
    extern const int KEY_HOME;
    extern const int KEY_END;
    extern const int KEY_INSERT;
    extern const int KEY_DELETE;
    extern const int KEY_BACKSPACE;
    extern const int KEY_SPACE;
    extern const int KEY_ENTER;
    extern const int KEY_ESCAPE;
    extern const int KEY_KP_ENTER;
    extern const int KEY_A;
    extern const int KEY_C;
    extern const int KEY_V;
    extern const int KEY_X;
    extern const int KEY_Y;
    extern const int KEY_Z;
    extern const int KEY_LEFT_CONTROL;
    extern const int KEY_RIGHT_CONTROL;
    extern const int KEY_LEFT_SHIFT;
    extern const int KEY_RIGHT_SHIFT;
    extern const int KEY_LEFT_ALT;
    extern const int KEY_RIGHT_ALT;
    extern const int KEY_LEFT_SUPER;
    extern const int KEY_RIGHT_SUPER;


    // query key state
    bool IsKeyPressed(int button);

    // up or down event
    bool IsKeyDown(int button);
    bool IsKeyUp(int button);

    // Drawing
    void ClearBackground(float r, float g, float b, float a);

    // Graphics State
    void PushGraphicState();
    void PopGraphicState();

    void SetDepthTest(bool enabled);

    enum class BlendMode {
        None,
        Alpha,
        Additive,
        Multiply,
        Screen,
    };
    void SetBlendMode(BlendMode blendMode);
    void SetScissor(bool enabled);
    void SetScissorRect(float x, float y, float width, float height);

    // Camera
    struct Camera3D {
        glm::vec3 origin;
        glm::vec3 lookat;
        glm::vec3 up = { 0.0f, 1.0f, 0.0f };
        float fovy     = glm::radians(45.0f); // vertical field of view
        float zNear = 0.01f;
        float zFar  = 1000.0f;
        bool zUp = false; // y up by default but you can use z up
    };
    void BeginCamera(Camera3D camera);
    void BeginCamera2DCanvas();
    void EndCamera();

    void UpdateCameraBlenderLike(
        Camera3D *camera,
        float wheel_sensitivity = 0.1f,
        float zoom_mouse_sensitivity = 0.002f,
        float rotate_sensitivity = 0.004f,
        float shift_sensitivity = 0.0006f
    );

    // Core Primitive Functions
    enum class PrimitiveMode {
        Points,
        Lines,
        LineStrip,
    };
    void     PrimBegin(PrimitiveMode mode, float width = 1.0f);
    uint32_t PrimVertex(glm::vec3 p, glm::u8vec3 c);
    void     PrimIndex(uint32_t index);
    void     PrimEnd();

    // Simple Functions
    void DrawLine(glm::vec3 p0, glm::vec3 p1, glm::u8vec3 c, float lineWidth = 1.0f);
    void DrawPoint(glm::vec3 p, glm::u8vec3 c, float pointSize = 1.0f);
    void DrawCircle(glm::vec3 o, glm::u8vec3 c, float radius, int vertexCount = 32, float lineWidth = 1.0f);

    enum class GridAxis {
        XY,
        XZ,
        YZ
    };
    void DrawGrid(GridAxis axis, float step, int blockCount, glm::u8vec3 c, float lineWidth = 1.0f);
    void DrawTube(glm::vec3 p0, glm::vec3 p1, float radius0, float radius1, glm::u8vec3 c, int vertexCount = 8, float lineWidth = 1.0f);
    void DrawArrow(glm::vec3 p0, glm::vec3 p1, float bodyRadius, glm::u8vec3 c, int vertexCount = 8, float lineWidth = 1.0f);
    void DrawXYZAxis(float length = 1.0f, float bodyRadius = 0.01f, int vertexCount = 8, float lineWidth = 1.0f);

    class ITextureRGBA8 {
    public:
        virtual ~ITextureRGBA8() {}
        virtual void upload(const Image2DRGBA8 &image) = 0;
        virtual void upload(const uint8_t *rgba, int width, int height) = 0;
        virtual int width() const = 0;
        virtual int height() const = 0;
    };
    ITextureRGBA8 *CreateTextureRGBA8();

    void     TriBegin(ITextureRGBA8 *texture);
    uint32_t TriVertex(glm::vec3 p, glm::vec2 uv, glm::u8vec4 c);
    void     TriIndex(uint32_t index);
    void     TriEnd();

    void BeginImGui();
    void EndImGui();
    bool IsImGuiUsingMouse();

    void SliderDirection(const char *label, glm::vec3 *dir, float minTheta = 0.0001f, float maxTheta = glm::pi<float>() - 0.0001f);
}