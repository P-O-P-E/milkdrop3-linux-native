#include "FadeOverlay.hpp"

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

struct FadeOverlay::Impl {
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
uniform float fadeOpacity;
out vec4 outputColor;
void main() {
    outputColor = vec4(0.0, 0.0, 0.0, fadeOpacity);
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

    void render(const float requestedOpacity) const {
        const float opacity = std::clamp(requestedOpacity, 0.0F, 1.0F);
        if (opacity <= 0.0F) {
            return;
        }

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
        uniform1f(opacityLocation, opacity);
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

FadeOverlay::FadeOverlay() : impl_(std::make_unique<Impl>()) {}
FadeOverlay::~FadeOverlay() = default;

void FadeOverlay::render(const float opacity) const { impl_->render(opacity); }

} // namespace md3
