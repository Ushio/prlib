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

/*
 Macros
*/
#include <intrin.h>
#define PR_ASSERT(ExpectTrue) if((ExpectTrue) == 0) { __debugbreak(); }

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
    bool ProcessSystem();

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

    extern int KEY_LEFT_SHIFT;
    extern int KEY_RIGHT_SHIFT;

    // query key state
    bool IsKeyPressed(int button);

    // up or down event
    bool IsKeyDown(int button);
    bool IsKeyUp(int button);

    // Random Number
    struct IRandom {
        virtual ~IRandom() {}

        // 0.0 <= x < 1.0
        float uniform();

        // a <= x < b
        float uniform(float a, float b);

        // a <= x < b
        int uniform(int a, int b);
    protected:
        /* float */
        // 0.0 <= x < 1.0
        virtual float uniform_float() = 0;

        /* A large integer enough to ignore modulo bias */
        virtual uint64_t uniform_integer() = 0;
    };

    IRandom *CreateRandomNumberGenerator(uint32_t seed);

    // Math

    template <class Real>
    struct LinearTransform {
        LinearTransform() :_a(Real(1.0)), _b(Real(0.0)) {}
        LinearTransform(Real a, Real b) :_a(a), _b(b) {}
        LinearTransform(Real inputMin, Real inputMax, Real outputMin, Real outputMax) {
            _a = (outputMax - outputMin) / (inputMax - inputMin);
            _b = outputMin - _a * inputMin;
        }
        Real evaluate(Real x) const {
            return std::fma(_a, x, _b);
        }
        LinearTransform<Real> inverse() const {
            return LinearTransform(Real(1.0f) / _a, -_b / _a);
        }
    private:
        Real _a;
        Real _b;
    };

    // Building an Orthonormal Basis, Revisited
    template <typename Real>
    inline void getOrthonormalBasis(const glm::tvec3<Real>& zaxis, glm::tvec3<Real> *xaxis, glm::tvec3<Real> *yaxis) {
        const Real sign = std::copysign(Real(1.0), zaxis.z);
        const Real a = Real(-1.0) / (sign + zaxis.z);
        const Real b = zaxis.x * zaxis.y * a;
        *xaxis = glm::tvec3<Real>(Real(1.0) + sign * zaxis.x * zaxis.x * a, sign * b, -sign * zaxis.x);
        *yaxis = glm::tvec3<Real>(b, sign + zaxis.y * zaxis.y * a, -zaxis.y);
    }

    float Radians(float degrees);
    float Degrees(float radians);

    // Drawing
    void ClearBackground(float r, float g, float b, float a);
    void SetDepthTest(bool enabled);

    // Camera
    struct Camera3D {
        glm::vec3 origin;
        glm::vec3 lookat;
        glm::vec3 up = { 0.0f, 1.0f, 0.0f };
        float fovy     = Radians(45.0f); // vertical field of view
        float zNear = 0.01f;
        float zFar  = 1000.0f;
    };
    void BeginCamera(Camera3D camera);
    void EndCamera();

    void UpdateCameraBlenderLike(
        Camera3D *camera,
        float wheel_sensitivity = 0.1f,
        float zoom_mouse_sensitivity = 0.002f,
        float rotate_sensitivity = 0.004f,
        float shift_sensitivity = 0.0006f
    );

    // Batch Module
    enum class PrimitiveMode {
        Points,
        Lines,
        LineStrip,
    };
    class Primitive {
    public:
        void clear();
        void add(glm::vec3 p, glm::u8vec3 c);
        void draw(PrimitiveMode mode, float width = 1.0f);
    private:
        std::vector<glm::vec3> _positions;
        std::vector<glm::u8vec3> _colors;
    };

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
    void DrawTube(glm::vec3 p0, glm::vec3 p1, float radius, glm::u8vec3 c, int vertexCount = 16, float lineWidth = 1.0f);
}