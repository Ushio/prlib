#pragma once

/*
 Please include me.
*/

#include <string>
#include <vector>
#include <functional>

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
#ifdef _WIN32
#include <intrin.h>
#define PR_ASSERT(ExpectTrue) if((ExpectTrue) == 0) { __debugbreak(); }
#else
#include <signal.h>
#define PR_ASSERT(ExpectTrue) if((ExpectTrue) == 0) { raise(SIGTRAP); }
#endif

#include "prp.hpp"
#include "prg.hpp"
#include "prtr.hpp"
#include "prth.hpp"

namespace pr {
    struct Config {
        int ScreenWidth = 1280;
        int ScreenHeight = 768;
        int SwapInterval = 1;
        int NumSamples = 4;
        int imguiFontSize = 20;
		std::string Title = "PrLib Window"; // UTF-8 encoded

        int64_t PersistentBufferLimitHint = 1024 * 1024 * 64;
    };
    void Initialize(Config config);
    void CleanUp();

    // for explicit call. it's ok to call without Initialize()
    void SetUTF8CodePage();

    int GetScreenWidth();
    int GetScreenHeight();

    /*
      - Swap BackBuffer
      - Process Window Event
      - return true if the window should close
    */
    bool NextFrame();

    // Realtime clock. this is available without Initialize
    double GetElapsedTime();

    // FrameBase clock
    double GetFrameTime();
    double GetFrameDeltaTime();
    double GetFrameRate();

    // Extension
	bool hasExternalObjectExtension();

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

    extern const int KEY_0;
    extern const int KEY_1;
    extern const int KEY_2;
    extern const int KEY_3;
    extern const int KEY_4;
    extern const int KEY_5;
    extern const int KEY_6;
    extern const int KEY_7;
    extern const int KEY_8;
    extern const int KEY_9;
    extern const int KEY_A;
    extern const int KEY_B;
    extern const int KEY_C;
    extern const int KEY_D;
    extern const int KEY_E;
    extern const int KEY_F;
    extern const int KEY_G;
    extern const int KEY_H;
    extern const int KEY_I;
    extern const int KEY_J;
    extern const int KEY_K;
    extern const int KEY_L;
    extern const int KEY_M;
    extern const int KEY_N;
    extern const int KEY_O;
    extern const int KEY_P;
    extern const int KEY_Q;
    extern const int KEY_R;
    extern const int KEY_S;
    extern const int KEY_T;
    extern const int KEY_U;
    extern const int KEY_V;
    extern const int KEY_W;
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

    // File Drop
    // UTF-8 encoded
    void SetFileDropCallback(std::function<void(std::vector<std::string>)> onFileDrop);

    // Drawing
    void ClearBackground(float r, float g, float b, float a);

    class ITexture;
    void ClearBackground(ITexture *texture);

    // Screen
    void CaptureScreen(Image2DRGBA8 *toImage, bool noAlpha = true);

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
    void SetScissorRect(int x, int y, int width, int height);

    // Camera
    struct Camera3D {
        glm::vec3 origin;
        glm::vec3 lookat;
        glm::vec3 up = { 0.0f, 1.0f, 0.0f };
        float fovy     = glm::radians(45.0f); // vertical field of view
        float zNear = 0.01f;
        float zFar  = 1000.0f;
        bool zUp = false; // y up by default but you can use z up.  But be careful, the camera parameter is always y up.
        float perspective = 1.0f;
    };

    // Object Transfrom
    void SetObjectTransform(glm::mat4 transform);
    glm::mat4 GetObjectTransform();
    void SetObjectIdentify();

    // These are stack-managemented.
    void GetCameraMatrix( Camera3D camera3d, glm::mat4 *proj, glm::mat4 *view, int width, int height );
    void BeginCamera(Camera3D camera);
    void BeginCamera2DCanvas();
    void BeginCameraNone();
    void EndCamera();

    glm::mat4 GetCurrentProjMatrix();
    glm::mat4 GetCurrentViewMatrix();

	// return true if the camera has modified.
    bool UpdateCameraBlenderLike(
        Camera3D *camera,
        float wheel_sensitivity = 0.1f,
        float zoom_mouse_sensitivity = 0.002f,
        float rotate_sensitivity = 0.004f,
        float shift_sensitivity = 0.0006f
    );

    Camera3D cameraFromEntity( const pr::FCameraEntity* e );

    // Core Primitive Functions ( Only performance, "Simple Functions" are recommended ) 
    // PrimVertex and PrimIndex is only allowed function between PrimBegin() and PrimEnd().
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
    void DrawCircle(glm::vec3 o, glm::vec3 dir, glm::u8vec3 c, float radius, int vertexCount = 32, float lineWidth = 1.0f);
    void DrawCube(glm::vec3 o, glm::vec3 size, glm::u8vec3 c, float lineWidth = 1.0f);
    void DrawAABB(glm::vec3 a, glm::vec3 b, glm::u8vec3 c, float lineWidth = 1.0f);

    enum class GridAxis {
        XY,
        XZ,
        YZ
    };
    enum class Orientation {
        X,
        Y,
        Z
    };
    void DrawGrid(GridAxis axis, float step, int blockCount, glm::u8vec3 c, float lineWidth = 1.0f);
	void DrawFreeGrid( glm::vec3 o, glm::vec3 dir0, glm::vec3 dir1, int blockCountHalf, glm::u8vec3 c, float lineWidth = 1.0f );
    void DrawTube(glm::vec3 p0, glm::vec3 p1, float radius0, float radius1, glm::u8vec3 c, int vertexCount = 8, float lineWidth = 1.0f);
    void DrawArrow(glm::vec3 p0, glm::vec3 p1, float bodyRadius, glm::u8vec3 c, int vertexCount = 8, float lineWidth = 1.0f);
    void DrawXYZAxis(float length = 1.0f, float bodyRadius = 0.01f, int vertexCount = 8, float lineWidth = 1.0f);
    void DrawSphere(glm::vec3 origin, float radius, glm::u8vec3 c, int rowCount = 8, int colCount = 8, glm::vec3 orientation = glm::vec3(0, 1, 0), float lineWidth = 1.0f);

    enum class TextureFilter {
        None,
        Linear,
    };
    class ITexture {
    public:
        virtual ~ITexture() {}
        virtual void upload(const Image2DRGBA8 &image) = 0;
        virtual void upload(const Image2DMono8 &image) = 0;
		virtual void upload(const Image2DRGBA32 &image) = 0;
        virtual void uploadAsRGBA8(const uint8_t *source, int width, int height) = 0;
		virtual void uploadAsMono8(const uint8_t *source, int width, int height) = 0;
		virtual void uploadAsRGBAF32(const float *source, int width, int height) = 0;
		virtual void fromInteropRGBA8( void* handle, int width, int height ) = 0;

        virtual int width() const = 0;
        virtual int height() const = 0;
        virtual void setFilter(TextureFilter filter) = 0;

        // internal use
        virtual void bind() const = 0;
    };
    ITexture *CreateTexture();

    void     TriBegin(const ITexture *texture);
    uint32_t TriVertex(glm::vec3 p, glm::vec2 uv, glm::u8vec4 c);
    void     TriIndex(uint32_t index);
    void     TriEnd();

    // world space
    void DrawText(glm::vec3 p_world, std::string text, float fontSize = 16.0f, glm::u8vec3 fontColor = { 16, 16, 240 }, float outlineWidth = 2.0f, glm::u8vec3 outlineColor = { 255, 255, 255 });
    
    // screen space
    void DrawTextScreen(float screen_x, float screen_y, std::string text, float fontSize = 16.0f, glm::u8vec3 fontColor = { 16, 16, 240 }, float outlineWidth = 2.0f, glm::u8vec3 outlineColor = { 255, 255, 255 });

    // by default, texts will be drawn at end of frame.
    // you can flash it immediately
    void FlashTextDrawing();

    void BeginImGui();
    void EndImGui();
    bool IsImGuiUsingMouse();

    // ImGui Helper

    // Polar
    void ImGuiSliderDirection(const char *label, glm::vec3 *dir, float minTheta = 0.0001f, float maxTheta = glm::pi<float>() - 0.0001f);

    // Draw and Manipulate
    void ManipulatePosition(const pr::Camera3D& camera, glm::vec3* v, float manipulatorSize);
}