#include "NativeFluid.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace md3 {
namespace {

template <typename Function>
Function loadFunction(const char* name) {
    void* address = SDL_GL_GetProcAddress(name);
    if (address == nullptr) {
        throw std::runtime_error("OpenGL function is unavailable: " + std::string(name));
    }
    static_assert(sizeof(Function) == sizeof(address));
    Function function{};
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

} // namespace

struct NativeFluid::Impl {
    using CreateShader = GLuint (*)(GLenum);
    using ShaderSource = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    using CompileShader = void (*)(GLuint);
    using GetShaderiv = void (*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using DeleteShader = void (*)(GLuint);
    using CreateProgram = GLuint (*)();
    using AttachShader = void (*)(GLuint, GLuint);
    using LinkProgram = void (*)(GLuint);
    using GetProgramiv = void (*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using DeleteProgram = void (*)(GLuint);
    using UseProgram = void (*)(GLuint);
    using GetUniformLocation = GLint (*)(GLuint, const GLchar*);
    using Uniform1f = void (*)(GLint, GLfloat);
    using GenVertexArrays = void (*)(GLsizei, GLuint*);
    using BindVertexArray = void (*)(GLuint);
    using DeleteVertexArrays = void (*)(GLsizei, const GLuint*);
    using DrawArrays = void (*)(GLenum, GLint, GLsizei);
    using BlendFuncSeparate = void (*)(GLenum, GLenum, GLenum, GLenum);
    using BlendEquationSeparate = void (*)(GLenum, GLenum);

    Impl()
        : createShader(loadFunction<CreateShader>("glCreateShader")),
          shaderSource(loadFunction<ShaderSource>("glShaderSource")),
          compileShaderFunction(loadFunction<CompileShader>("glCompileShader")),
          getShaderiv(loadFunction<GetShaderiv>("glGetShaderiv")),
          getShaderInfoLog(loadFunction<GetShaderInfoLog>("glGetShaderInfoLog")),
          deleteShader(loadFunction<DeleteShader>("glDeleteShader")),
          createProgram(loadFunction<CreateProgram>("glCreateProgram")),
          attachShader(loadFunction<AttachShader>("glAttachShader")),
          linkProgramFunction(loadFunction<LinkProgram>("glLinkProgram")),
          getProgramiv(loadFunction<GetProgramiv>("glGetProgramiv")),
          getProgramInfoLog(loadFunction<GetProgramInfoLog>("glGetProgramInfoLog")),
          deleteProgram(loadFunction<DeleteProgram>("glDeleteProgram")),
          useProgram(loadFunction<UseProgram>("glUseProgram")),
          getUniformLocation(loadFunction<GetUniformLocation>("glGetUniformLocation")),
          uniform1f(loadFunction<Uniform1f>("glUniform1f")),
          genVertexArrays(loadFunction<GenVertexArrays>("glGenVertexArrays")),
          bindVertexArray(loadFunction<BindVertexArray>("glBindVertexArray")),
          deleteVertexArrays(loadFunction<DeleteVertexArrays>("glDeleteVertexArrays")),
          drawArrays(loadFunction<DrawArrays>("glDrawArrays")),
          blendFuncSeparate(loadFunction<BlendFuncSeparate>("glBlendFuncSeparate")),
          blendEquationSeparate(loadFunction<BlendEquationSeparate>("glBlendEquationSeparate")) {
        constexpr const char* vertexSource = R"(
#version 330 core
void main() {
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";
        constexpr const char* fragmentSource = R"(
#version 330 core
uniform float fadeOpacity; // Continuous look coordinate, 0..3.
uniform float clockTime;
uniform float audioEnergy;
uniform float screenAspect;
uniform float viewportHeight;
out vec4 outputColor;
vec3 palette(float look, float core) {
    vec3 amber=mix(vec3(0.55,0.025,0.003),vec3(1.0,0.65,0.12),core);
    vec3 violet=mix(vec3(0.12,0.015,0.45),vec3(0.72,0.25,0.95),core);
    vec3 ocean=mix(vec3(0.005,0.12,0.22),vec3(0.10,0.85,0.92),core);
    vec3 glass=mix(vec3(0.30,0.008,0.003),vec3(0.95,0.28,0.035),core);
    if (look<1.0) return mix(amber,violet,look);
    if (look<2.0) return mix(violet,ocean,look-1.0);
    return mix(ocean,glass,look-2.0);
}
void main() {
    vec2 p=(gl_FragCoord.xy/viewportHeight-vec2(screenAspect,1.0)*0.5);
    float t=clockTime*0.22;
    float look=clamp(fadeOpacity,0.0,3.0);
    p+=0.028*sin(p.yx*7.0+vec2(t,-t));
    float field=0.0;
    for(int i=0;i<6;i++) {
        float n=float(i);
        vec2 c=vec2(0.32*sin(t*(0.61+n*0.07)+n*2.1),
                    0.36*sin(t*(0.73+n*0.05)+n*1.7));
        vec2 d=(p-c)/vec2(1.0,1.0+0.18*sin(look+n));
        field+=(0.017+0.003*sin(n+t))/(dot(d,d)+0.006);
    }
    field*=1.0+0.12*audioEnergy;
    float body=smoothstep(1.0,1.6,field);
    float core=smoothstep(1.6,3.4,field);
    float rim=body*(1.0-core);
    vec3 col=palette(look,core);
    col+=rim*0.12*pow(0.5+0.5*sin(p.x*9.0-p.y*6.0+t),4.0);
    vec3 bg=palette(look,0.0)*0.055;
    outputColor=vec4(mix(bg,col,body),1.0);
}

)";

        GLuint vertexShader = 0;
        GLuint fragmentShader = 0;
        try {
            vertexShader = compile(GL_VERTEX_SHADER, vertexSource);
            fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);
            program = createProgram();
            attachShader(program, vertexShader);
            attachShader(program, fragmentShader);
            linkProgramFunction(program);

            GLint linked = GL_FALSE;
            getProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE) {
                throw std::runtime_error("Unable to link fade overlay shader: " + programLog(program));
            }
            opacityLocation = getUniformLocation(program, "fadeOpacity");
            if (opacityLocation < 0) {
                throw std::runtime_error("Fade overlay shader is missing its opacity uniform");
            }
            genVertexArrays(1, &vertexArray);
        } catch (...) {
            if (program != 0) {
                deleteProgram(program);
                program = 0;
            }
            if (vertexShader != 0) {
                deleteShader(vertexShader);
            }
            if (fragmentShader != 0) {
                deleteShader(fragmentShader);
            }
            throw;
        }
        deleteShader(vertexShader);
        deleteShader(fragmentShader);
    }

    ~Impl() {
        if (vertexArray != 0) {
            deleteVertexArrays(1, &vertexArray);
        }
        if (program != 0) {
            deleteProgram(program);
        }
    }

    GLuint compile(const GLenum type, const char* source) const {
        const GLuint shader = createShader(type);
        shaderSource(shader, 1, &source, nullptr);
        compileShaderFunction(shader);
        GLint compiled = GL_FALSE;
        getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            const auto message = shaderLog(shader);
            deleteShader(shader);
            throw std::runtime_error("Unable to compile fade overlay shader: " + message);
        }
        return shader;
    }

    std::string shaderLog(const GLuint shader) const {
        GLint length = 0;
        getShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
        getShaderInfoLog(shader, length, nullptr, log.data());
        return log;
    }

    std::string programLog(const GLuint value) const {
        GLint length = 0;
        getProgramiv(value, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
        getProgramInfoLog(value, length, nullptr, log.data());
        return log;
    }

    void render(float time, float look, float energy, float aspect) const {

        GLint previousProgram = 0;
        GLint previousVertexArray = 0;
        GLint previousBlendSourceRgb = 0;
        GLint previousBlendDestinationRgb = 0;
        GLint previousBlendSourceAlpha = 0;
        GLint previousBlendDestinationAlpha = 0;
        GLint previousBlendEquationRgb = 0;
        GLint previousBlendEquationAlpha = 0;
        GLboolean previousDepthMask = GL_TRUE;
        GLboolean previousColorMask[4]{GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
        glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSourceRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDestinationRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSourceAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDestinationAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &previousBlendEquationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &previousBlendEquationAlpha);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);

        const bool blendWasEnabled = glIsEnabled(GL_BLEND) == GL_TRUE;
        const bool depthWasEnabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
        const bool scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
        const bool cullWasEnabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;

        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        blendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        useProgram(program);
        uniform1f(opacityLocation, look);
        uniform1f(getUniformLocation(program, "clockTime"), time);
        uniform1f(getUniformLocation(program, "audioEnergy"), energy);
        uniform1f(getUniformLocation(program, "screenAspect"), aspect);
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        uniform1f(getUniformLocation(program, "viewportHeight"), static_cast<float>(std::max(1, viewport[3])));
        bindVertexArray(vertexArray);
        drawArrays(GL_TRIANGLES, 0, 3);

        bindVertexArray(static_cast<GLuint>(previousVertexArray));
        useProgram(static_cast<GLuint>(previousProgram));
        blendEquationSeparate(static_cast<GLenum>(previousBlendEquationRgb),
                              static_cast<GLenum>(previousBlendEquationAlpha));
        blendFuncSeparate(static_cast<GLenum>(previousBlendSourceRgb),
                          static_cast<GLenum>(previousBlendDestinationRgb),
                          static_cast<GLenum>(previousBlendSourceAlpha),
                          static_cast<GLenum>(previousBlendDestinationAlpha));
        glDepthMask(previousDepthMask);
        glColorMask(previousColorMask[0], previousColorMask[1], previousColorMask[2], previousColorMask[3]);
        blendWasEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        depthWasEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        scissorWasEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
        cullWasEnabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    }

    CreateShader createShader;
    ShaderSource shaderSource;
    CompileShader compileShaderFunction;
    GetShaderiv getShaderiv;
    GetShaderInfoLog getShaderInfoLog;
    DeleteShader deleteShader;
    CreateProgram createProgram;
    AttachShader attachShader;
    LinkProgram linkProgramFunction;
    GetProgramiv getProgramiv;
    GetProgramInfoLog getProgramInfoLog;
    DeleteProgram deleteProgram;
    UseProgram useProgram;
    GetUniformLocation getUniformLocation;
    Uniform1f uniform1f;
    GenVertexArrays genVertexArrays;
    BindVertexArray bindVertexArray;
    DeleteVertexArrays deleteVertexArrays;
    DrawArrays drawArrays;
    BlendFuncSeparate blendFuncSeparate;
    BlendEquationSeparate blendEquationSeparate;
    GLuint program{0};
    GLuint vertexArray{0};
    GLint opacityLocation{-1};
};

NativeFluid::NativeFluid() : impl_(std::make_unique<Impl>()) {}
NativeFluid::~NativeFluid() = default;

void NativeFluid::render(float time, float look, float energy, float aspect) const { impl_->render(time, look, energy, aspect); }

} // namespace md3
