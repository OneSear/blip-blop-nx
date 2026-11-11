// The display of the frames with OpenGL: the frame is uploaded to a texture
// and drawn by a shader, the filter's, at the size core::gfx::viewport()
// gives. The game still draws its frames in software (DEC-01); only this
// last step, to the screen, runs on the GPU. OpenGL ES 2.0 on the Switch,
// OpenGL 2.1 on macOS and Linux: the shaders are written in the language
// both understand (GLSL ES 1.00 and GLSL 1.20), and the functions are loaded
// from the context, so that no OpenGL header or library is needed beyond
// SDL's.

#include "gl_display.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bb/core/gfx/viewport.hpp"
#include "bb/core/log.hpp"

namespace bb::platform::sdl2 {

namespace {

namespace gfx = core::gfx;

// The OpenGL types and constants used here (OpenGL ES 2.0 and OpenGL 2.1
// agree on them).
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLsizeiptr = std::ptrdiff_t;
using GLubyte = unsigned char;

constexpr GLenum kNoError = 0;
constexpr GLenum kTexture2D = 0x0DE1;
constexpr GLenum kTextureMagFilter = 0x2800;
constexpr GLenum kTextureMinFilter = 0x2801;
constexpr GLenum kTextureWrapS = 0x2802;
constexpr GLenum kTextureWrapT = 0x2803;
constexpr GLint kNearest = 0x2600;
constexpr GLint kLinear = 0x2601;
constexpr GLint kClampToEdge = 0x812F;
constexpr GLenum kRgba = 0x1908;
constexpr GLenum kUnsignedByte = 0x1401;
constexpr GLenum kUnpackAlignment = 0x0CF5;
constexpr GLenum kPackAlignment = 0x0D05;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kFragmentShaderType = 0x8B30;
constexpr GLenum kVertexShaderType = 0x8B31;
constexpr GLenum kCompileStatus = 0x8B81;
constexpr GLenum kLinkStatus = 0x8B82;
constexpr GLenum kInfoLogLength = 0x8B84;
constexpr GLenum kArrayBuffer = 0x8892;
constexpr GLenum kStaticDraw = 0x88E4;
constexpr GLenum kFloat = 0x1406;
constexpr GLenum kTriangleStrip = 0x0005;
constexpr GLbitfield kColorBufferBit = 0x00004000;
constexpr GLenum kFramebuffer = 0x8D40;
constexpr GLenum kColorAttachment0 = 0x8CE0;
constexpr GLenum kFramebufferComplete = 0x8CD5;
constexpr GLenum kBlend = 0x0BE2;
constexpr GLenum kDepthTest = 0x0B71;
constexpr GLenum kScissorTest = 0x0C11;
constexpr GLenum kCullFace = 0x0B44;
constexpr GLenum kRenderer = 0x1F01;
constexpr GLenum kVersion = 0x1F02;

// The functions, from the context (SDL_GL_GetProcAddress).
struct Gl {
    GLenum (*getError)() = nullptr;
    const GLubyte* (*getString)(GLenum) = nullptr;
    void (*viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (*clearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (*clear)(GLbitfield) = nullptr;
    void (*disable)(GLenum) = nullptr;
    void (*genTextures)(GLsizei, GLuint*) = nullptr;
    void (*deleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (*bindTexture)(GLenum, GLuint) = nullptr;
    void (*texParameteri)(GLenum, GLenum, GLint) = nullptr;
    void (*texImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (*texSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                          const void*) = nullptr;
    void (*pixelStorei)(GLenum, GLint) = nullptr;
    void (*activeTexture)(GLenum) = nullptr;
    GLuint (*createShader)(GLenum) = nullptr;
    void (*shaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (*compileShader)(GLuint) = nullptr;
    void (*getShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*getShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*deleteShader)(GLuint) = nullptr;
    GLuint (*createProgram)() = nullptr;
    void (*attachShader)(GLuint, GLuint) = nullptr;
    void (*bindAttribLocation)(GLuint, GLuint, const GLchar*) = nullptr;
    void (*linkProgram)(GLuint) = nullptr;
    void (*getProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*getProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*deleteProgram)(GLuint) = nullptr;
    void (*useProgram)(GLuint) = nullptr;
    GLint (*getUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (*uniform1i)(GLint, GLint) = nullptr;
    void (*uniform2f)(GLint, GLfloat, GLfloat) = nullptr;
    void (*genBuffers)(GLsizei, GLuint*) = nullptr;
    void (*deleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (*bindBuffer)(GLenum, GLuint) = nullptr;
    void (*bufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (*vertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (*enableVertexAttribArray)(GLuint) = nullptr;
    void (*drawArrays)(GLenum, GLint, GLsizei) = nullptr;
    void (*readPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
    void (*genFramebuffers)(GLsizei, GLuint*) = nullptr;
    void (*deleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
    void (*bindFramebuffer)(GLenum, GLuint) = nullptr;
    void (*framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
    GLenum (*checkFramebufferStatus)(GLenum) = nullptr;

    // Loads every function; the name of the first one missing, or "".
    std::string load() {
        std::string missing;
        const auto get = [&missing](auto& function, const char* name, const char* other = nullptr) {
            void* address = SDL_GL_GetProcAddress(name);
            if (address == nullptr && other != nullptr) {
                address = SDL_GL_GetProcAddress(other);
            }
            if (address == nullptr && missing.empty()) {
                missing = name;
            }
            // (A function pointer from an object pointer, without a cast
            // that the warnings refuse.)
            std::memcpy(&function, &address, sizeof function);
        };
        get(getError, "glGetError");
        get(getString, "glGetString");
        get(viewport, "glViewport");
        get(clearColor, "glClearColor");
        get(clear, "glClear");
        get(disable, "glDisable");
        get(genTextures, "glGenTextures");
        get(deleteTextures, "glDeleteTextures");
        get(bindTexture, "glBindTexture");
        get(texParameteri, "glTexParameteri");
        get(texImage2D, "glTexImage2D");
        get(texSubImage2D, "glTexSubImage2D");
        get(pixelStorei, "glPixelStorei");
        get(activeTexture, "glActiveTexture");
        get(createShader, "glCreateShader");
        get(shaderSource, "glShaderSource");
        get(compileShader, "glCompileShader");
        get(getShaderiv, "glGetShaderiv");
        get(getShaderInfoLog, "glGetShaderInfoLog");
        get(deleteShader, "glDeleteShader");
        get(createProgram, "glCreateProgram");
        get(attachShader, "glAttachShader");
        get(bindAttribLocation, "glBindAttribLocation");
        get(linkProgram, "glLinkProgram");
        get(getProgramiv, "glGetProgramiv");
        get(getProgramInfoLog, "glGetProgramInfoLog");
        get(deleteProgram, "glDeleteProgram");
        get(useProgram, "glUseProgram");
        get(getUniformLocation, "glGetUniformLocation");
        get(uniform1i, "glUniform1i");
        get(uniform2f, "glUniform2f");
        get(genBuffers, "glGenBuffers");
        get(deleteBuffers, "glDeleteBuffers");
        get(bindBuffer, "glBindBuffer");
        get(bufferData, "glBufferData");
        get(vertexAttribPointer, "glVertexAttribPointer");
        get(enableVertexAttribArray, "glEnableVertexAttribArray");
        get(drawArrays, "glDrawArrays");
        get(readPixels, "glReadPixels");
        get(genFramebuffers, "glGenFramebuffers", "glGenFramebuffersEXT");
        get(deleteFramebuffers, "glDeleteFramebuffers", "glDeleteFramebuffersEXT");
        get(bindFramebuffer, "glBindFramebuffer", "glBindFramebufferEXT");
        get(framebufferTexture2D, "glFramebufferTexture2D", "glFramebufferTexture2DEXT");
        get(checkFramebufferStatus, "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
        return missing;
    }
};

//-----------------------------------------------------------------------------
// The shaders
//-----------------------------------------------------------------------------

// Every shader draws one quad over the picture; vTexCoord runs over the frame
// with its first row at 0, as the texture holds it.
const char* const kVertexSource = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

// SHARP and SMOOTH (the texture's own nearest or bilinear filtering), and
// the second pass of XBR.
const char* const kPlainSource = R"(
uniform sampler2D uSource;
varying vec2 vTexCoord;
void main() {
    gl_FragColor = vec4(texture2D(uSource, vTexCoord).rgb, 1.0);
}
)";

// CRISP: each frame pixel enlarged uScale times with sharp edges, then drawn
// at the picture's size with bilinear filtering (what the SDL display does
// with a texture uScale times the frame), computed for each screen pixel: in
// the middle of a frame pixel its colour, at its edges a blend as wide as a
// screen pixel (the texture filters bilinearly).
const char* const kCrispSource = R"(
uniform sampler2D uSource;
uniform vec2 uSourceSize;
uniform vec2 uScale;
varying vec2 vTexCoord;
void main() {
    vec2 texel = vTexCoord * uSourceSize;
    vec2 base = floor(texel);
    vec2 inside = texel - base - 0.5;
    vec2 range = 0.5 - 0.5 / uScale;
    vec2 shifted = (inside - clamp(inside, -range, range)) * uScale + 0.5;
    gl_FragColor = vec4(texture2D(uSource, (base + shifted) / uSourceSize).rgb, 1.0);
}
)";

// XBR, first pass: the frame enlarged uScale times (a whole factor), its
// edges smoothed. In the manner of Hyllian's xBR (levels 1 and 2), written
// for this game: each screen pixel is in a quarter of a frame pixel E, the
// one of a corner; xBR's rule compares the colour differences across and
// along the corner's diagonal (5x5 pixels around E) and finds whether an
// edge passes between E and the corner's pixel I, at 45 degrees, or at half
// or twice that slope; beyond the edge's line the pixel takes the colour of
// the nearer of E's two neighbours on that corner, F or H, with a blend one
// screen pixel wide. Differences are weighted towards brightness (Y, U, V).
const char* const kXbrSource = R"(
uniform sampler2D uSource;
uniform vec2 uSourceSize;
uniform vec2 uScale;
varying vec2 vTexCoord;

vec3 at(vec2 cell) {
    return texture2D(uSource, (cell + 0.5) / uSourceSize).rgb;
}

float diff(vec3 a, vec3 b) {
    vec3 d = a - b;
    float y = dot(d, vec3(0.299, 0.587, 0.114));
    float u = dot(d, vec3(-0.169, -0.331, 0.5));
    float v = dot(d, vec3(0.5, -0.419, -0.081));
    return 48.0 * abs(y) + 7.0 * abs(u) + 6.0 * abs(v);
}

bool same(vec3 a, vec3 b) {
    return diff(a, b) < 0.6;
}

// 0 before the line n.p = 0.5, 1 a screen pixel beyond it.
float beyond(vec2 p, vec2 n, float pixel) {
    float halfWidth = 0.5 * pixel * length(n);
    return smoothstep(0.5 - halfWidth, 0.5 + halfWidth, dot(n, p));
}

void main() {
    vec2 texel = vTexCoord * uSourceSize;
    vec2 cell = floor(texel);
    vec2 inside = texel - cell - 0.5;
    vec2 dir = vec2(inside.x >= 0.0 ? 1.0 : -1.0, inside.y >= 0.0 ? 1.0 : -1.0);
    vec2 p = inside * dir;  // in the corner's quarter, from 0 to 0.5

    vec3 E = at(cell);
    vec3 B = at(cell + vec2(0.0, -dir.y));
    vec3 C = at(cell + vec2(dir.x, -dir.y));
    vec3 D = at(cell + vec2(-dir.x, 0.0));
    vec3 F = at(cell + vec2(dir.x, 0.0));
    vec3 G = at(cell + vec2(-dir.x, dir.y));
    vec3 H = at(cell + vec2(0.0, dir.y));
    vec3 I = at(cell + dir);
    vec3 F4 = at(cell + vec2(2.0 * dir.x, 0.0));
    vec3 I4 = at(cell + vec2(2.0 * dir.x, dir.y));
    vec3 H5 = at(cell + vec2(0.0, 2.0 * dir.y));
    vec3 I5 = at(cell + vec2(dir.x, 2.0 * dir.y));

    float across = diff(E, C) + diff(E, G) + diff(I, F4) + diff(I, H5) + 4.0 * diff(H, F);
    float along = diff(H, D) + diff(H, I5) + diff(F, I4) + diff(F, B) + 4.0 * diff(E, I);
    bool edge = across < along && !same(E, F) && !same(E, H);
    bool shallow = edge && 2.0 * diff(F, G) <= diff(H, C) && !same(E, G) && !same(D, G);
    bool steep = edge && 2.0 * diff(H, C) <= diff(F, G) && !same(E, C) && !same(B, C);

    float pixel = 1.0 / uScale.x;
    float blend = max(float(edge) * beyond(p, vec2(1.0, 1.0), pixel),
                      max(float(shallow) * beyond(p, vec2(1.0, 2.0), pixel),
                          float(steep) * beyond(p, vec2(2.0, 1.0), pixel)));
    vec3 other = diff(E, F) <= diff(E, H) ? F : H;
    gl_FragColor = vec4(mix(E, other, blend), 1.0);
}
)";

// CRT: the frame as a tube showed it. The picture curves a little, as the
// glass did, its corners darker and rounded. Down, the light of the two
// nearest rows' beams: narrow bells, wider where brighter, so that dark lines
// run between the rows even on bright colours; across, a little soft, as a
// beam is. Then an aperture grille (columns weaker in two colours of three;
// the stripes wider on a large screen, so that they show), a glow of the
// rows' plain colour, and the light the lines and the grille took back.
// Computed in linear light.
const char* const kCrtSource = R"(
uniform sampler2D uSource;
uniform vec2 uSourceSize;
uniform vec2 uOutputSize;
varying vec2 vTexCoord;

vec2 curve(vec2 uv) {
    vec2 c = uv * 2.0 - 1.0;
    c *= vec2(1.0 + c.y * c.y * 0.03, 1.0 + c.x * c.x * 0.04);
    return c * 0.5 + 0.5;
}

vec3 row(float y, float x) {
    vec3 c = texture2D(uSource, vec2(x, (y + 0.5) / uSourceSize.y)).rgb;
    return c * c;
}

vec3 beam(float dist, vec3 colour) {
    vec3 width = 0.15 + 0.13 * sqrt(colour);
    vec3 d = dist / width;
    return exp(-d * d);
}

void main() {
    vec2 uv = curve(vTexCoord);
    vec2 pixel = 1.0 / uOutputSize;
    vec2 inside = smoothstep(vec2(0.0), 2.0 * pixel, uv) * smoothstep(vec2(0.0), 2.0 * pixel, 1.0 - uv);
    if (inside.x * inside.y <= 0.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float y = uv.y * uSourceSize.y - 0.5;
    float above = floor(y);
    float t = y - above;
    vec3 a = row(above, uv.x);
    vec3 b = row(above + 1.0, uv.x);
    vec3 scan = a * beam(t, a) + b * beam(1.0 - t, b);
    vec3 glow = mix(a, b, t);

    float triad = max(1.0, floor(uOutputSize.y / 700.0));
    float column = mod(floor(gl_FragCoord.x / triad), 3.0);
    vec3 mask = column < 1.0 ? vec3(1.0, 0.68, 0.68) : (column < 2.0 ? vec3(0.68, 1.0, 0.68) : vec3(0.68, 0.68, 1.0));
    vec3 colour = scan * mask * 2.2 + glow * 0.06;

    vec2 edge = uv * (1.0 - uv);
    colour *= pow(clamp(edge.x * edge.y * 15.0, 0.0, 1.0), 0.2) * inside.x * inside.y;
    gl_FragColor = vec4(sqrt(clamp(colour, 0.0, 1.0)), 1.0);
}
)";

// The quad over the whole target, as a triangle strip: x, y, then the
// frame's coordinates. The first draws the frame upright on the screen; the
// second samples a texture that a first pass drew (its rows from the bottom).
const GLfloat kQuad[] = {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
                         -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f};
const GLfloat kQuadFromPass[] = {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
                                 -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};

constexpr GLuint kPositionAttribute = 0;
constexpr GLuint kTexCoordAttribute = 1;

struct Program {
    GLuint id = 0;
    GLint source = -1;
    GLint sourceSize = -1;
    GLint scale = -1;
    GLint outputSize = -1;
};

// A texture and the framebuffer that draws into it (a pass's target).
struct Target {
    GLuint texture = 0;
    GLuint framebuffer = 0;
    int width = 0;
    int height = 0;
};

class GlDisplay final : public Sdl2DisplayBase {
public:
    GlDisplay(SDL_Window* window, SDL_GLContext context, const Gl& gl, bool es)
        : window_(window), context_(context), gl_(gl), es_(es) {}

    ~GlDisplay() override {
        SDL_GL_MakeCurrent(window_, context_);
        for (Target* t : {&xbrPass_, &capture_}) {
            release(*t);
        }
        if (frameTexture_ != 0) {
            gl_.deleteTextures(1, &frameTexture_);
        }
        for (Program* p : {&plain_, &crisp_, &xbr_, &crt_}) {
            if (p->id != 0) {
                gl_.deleteProgram(p->id);
            }
        }
        const GLuint buffers[] = {quad_, quadFromPass_};
        gl_.deleteBuffers(2, buffers);
        SDL_GL_DeleteContext(context_);
        SDL_DestroyWindow(window_);
    }

    GlDisplay(const GlDisplay&) = delete;
    GlDisplay& operator=(const GlDisplay&) = delete;

    // Builds the shaders and the quads; false, with error, when one does
    // not build.
    bool start(std::string& error) {
        version_ = text(gl_.getString(kVersion)) + " (" + text(gl_.getString(kRenderer)) + ")";
        vsync_ = SDL_GL_SetSwapInterval(1) == 0;
        for (const GLenum capability : {kBlend, kDepthTest, kScissorTest, kCullFace}) {
            gl_.disable(capability);
        }
        gl_.pixelStorei(kUnpackAlignment, 4);
        gl_.pixelStorei(kPackAlignment, 4);
        if (!build(plain_, kPlainSource, "plain", error) || !build(crisp_, kCrispSource, "crisp", error) ||
            !build(xbr_, kXbrSource, "xbr", error) || !build(crt_, kCrtSource, "crt", error)) {
            return false;
        }
        GLuint buffers[2] = {0, 0};
        gl_.genBuffers(2, buffers);
        quad_ = buffers[0];
        quadFromPass_ = buffers[1];
        gl_.bindBuffer(kArrayBuffer, quad_);
        gl_.bufferData(kArrayBuffer, static_cast<GLsizeiptr>(sizeof kQuad), kQuad, kStaticDraw);
        gl_.bindBuffer(kArrayBuffer, quadFromPass_);
        gl_.bufferData(kArrayBuffer, static_cast<GLsizeiptr>(sizeof kQuadFromPass), kQuadFromPass,
                       kStaticDraw);
        gl_.genTextures(1, &frameTexture_);
        SDL_GL_GetDrawableSize(window_, &outputWidth_, &outputHeight_);
        const GLenum status = gl_.getError();
        if (status != kNoError) {
            error = "OpenGL error " + std::to_string(status) + " while starting";
            return false;
        }
        return true;
    }

    void present(const gfx::Pixmap& frame) override {
        upload(frame);
        int w = 0;
        int h = 0;
        SDL_GL_GetDrawableSize(window_, &w, &h);
        if (w != outputWidth_ || h != outputHeight_) {
            outputWidth_ = w;
            outputHeight_ = h;
            core::Log::info("Display: " + describe());
        }
        draw(0, w, h);
        SDL_GL_SwapWindow(window_);
    }

    void setScaling(bool integer, gfx::Filter filter) override {
        integer_ = integer;
        filter_ = filter;
    }

    int refreshRate() const override {
        SDL_DisplayMode mode;
        return SDL_GetWindowDisplayMode(window_, &mode) == 0 ? mode.refresh_rate : 0;
    }

    std::string describe() const override {
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        const gfx::Viewport view = viewport(outputWidth_, outputHeight_);
        // (OpenGL ES names itself: "OpenGL ES 2.0 Mesa ...".)
        std::string out = (version_.rfind("OpenGL", 0) == 0 ? "" : "OpenGL ") + version_ + ", window " +
                          std::to_string(w) + "x" + std::to_string(h) + ", drawn at " +
                          std::to_string(outputWidth_) + "x" + std::to_string(outputHeight_) +
                          " pixels, picture " + std::to_string(view.picture.w) + "x" +
                          std::to_string(view.picture.h) + " at " + std::to_string(view.picture.x) + "," +
                          std::to_string(view.picture.y) + ", scaling " + (integer_ ? "integer" : "fit") +
                          ", filter " + gfx::filterName(filter_);
        if (filter_ == gfx::Filter::Xbr) {
            out += " (x" + std::to_string(xbrScale(view)) + " first)";
        }
        return out;
    }

    bool vsync() const override { return vsync_; }

    bool capture(gfx::Pixmap& out, int width, int height) override {
        SDL_GL_MakeCurrent(window_, context_);
        if (frameWidth_ == 0 || width <= 0 || height <= 0 || !prepare(capture_, width, height, kNearest)) {
            return false;
        }
        draw(capture_.framebuffer, width, height);
        std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        gl_.readPixels(0, 0, width, height, kRgba, kUnsignedByte, pixels.data());
        gl_.bindFramebuffer(kFramebuffer, 0);
        // The framebuffer's rows from the bottom; RGBA bytes are the
        // pixmap's 0xAABBGGRR.
        out = gfx::Pixmap(width, height);
        for (int y = 0; y < height; ++y) {
            std::memcpy(
                out.row(y),
                pixels.data() + static_cast<std::size_t>(height - 1 - y) * static_cast<std::size_t>(width),
                static_cast<std::size_t>(width) * sizeof(std::uint32_t));
        }
        return gl_.getError() == kNoError;
    }

    bool readBack(gfx::Pixmap& out) override { return capture(out, outputWidth_, outputHeight_); }

private:
    static std::string text(const GLubyte* s) {
        return s == nullptr ? "?" : reinterpret_cast<const char*>(s);
    }

    gfx::Viewport viewport(int width, int height) const {
        return gfx::viewport(frameWidth_, frameHeight_, width, height, integer_, filter_);
    }

    // XBR's whole factor: the next one at or above the picture's scale, 2 or
    // more (x2 in handheld mode, x3 docked).
    int xbrScale(const gfx::Viewport& view) const {
        if (frameWidth_ == 0) {
            return 2;
        }
        return std::max(2, (view.picture.w + frameWidth_ - 1) / frameWidth_);
    }

    bool build(Program& program, const char* fragment, const char* name, std::string& error) {
        // The same source for GLSL ES 1.00 and GLSL 1.20: precision for the
        // first, none for the second.
        const std::string preamble = es_ ? "#version 100\n#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
                                           "precision highp float;\n#else\nprecision mediump float;\n#endif\n"
                                         : "#version 120\n#define highp\n#define mediump\n#define lowp\n";
        const GLuint vertex = compile(kVertexSource, kVertexShaderType, preamble, name, error);
        const GLuint pixel = vertex == 0 ? 0 : compile(fragment, kFragmentShaderType, preamble, name, error);
        if (pixel == 0) {
            if (vertex != 0) {
                gl_.deleteShader(vertex);
            }
            return false;
        }
        program.id = gl_.createProgram();
        gl_.attachShader(program.id, vertex);
        gl_.attachShader(program.id, pixel);
        gl_.bindAttribLocation(program.id, kPositionAttribute, "aPosition");
        gl_.bindAttribLocation(program.id, kTexCoordAttribute, "aTexCoord");
        gl_.linkProgram(program.id);
        gl_.deleteShader(vertex);
        gl_.deleteShader(pixel);
        GLint linked = 0;
        gl_.getProgramiv(program.id, kLinkStatus, &linked);
        if (linked == 0) {
            GLint length = 0;
            gl_.getProgramiv(program.id, kInfoLogLength, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            gl_.getProgramInfoLog(program.id, length, nullptr, log.data());
            error = std::string("the ") + name + " shader does not link: " + log;
            return false;
        }
        program.source = gl_.getUniformLocation(program.id, "uSource");
        program.sourceSize = gl_.getUniformLocation(program.id, "uSourceSize");
        program.scale = gl_.getUniformLocation(program.id, "uScale");
        program.outputSize = gl_.getUniformLocation(program.id, "uOutputSize");
        return true;
    }

    GLuint compile(const char* source, GLenum type, const std::string& preamble, const char* name,
                   std::string& error) {
        const GLuint shader = gl_.createShader(type);
        const GLchar* parts[] = {preamble.c_str(), source};
        gl_.shaderSource(shader, 2, parts, nullptr);
        gl_.compileShader(shader);
        GLint compiled = 0;
        gl_.getShaderiv(shader, kCompileStatus, &compiled);
        if (compiled == 0) {
            GLint length = 0;
            gl_.getShaderiv(shader, kInfoLogLength, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            gl_.getShaderInfoLog(shader, length, nullptr, log.data());
            error = std::string("the ") + name + " shader does not compile: " + log;
            gl_.deleteShader(shader);
            return 0;
        }
        return shader;
    }

    // The frame in its texture (a new texture when its size changes).
    void upload(const gfx::Pixmap& frame) {
        gl_.activeTexture(kTexture0);
        gl_.bindTexture(kTexture2D, frameTexture_);
        if (frame.width() != frameWidth_ || frame.height() != frameHeight_) {
            frameWidth_ = frame.width();
            frameHeight_ = frame.height();
            gl_.texImage2D(kTexture2D, 0, static_cast<GLint>(kRgba), frameWidth_, frameHeight_, 0, kRgba,
                           kUnsignedByte, nullptr);
            gl_.texParameteri(kTexture2D, kTextureWrapS, kClampToEdge);
            gl_.texParameteri(kTexture2D, kTextureWrapT, kClampToEdge);
            core::Log::info("Display: " + describe());
        }
        gl_.texSubImage2D(kTexture2D, 0, 0, 0, frameWidth_, frameHeight_, kRgba, kUnsignedByte, frame.data());
    }

    void setFiltering(GLuint texture, GLint filtering) {
        gl_.activeTexture(kTexture0);
        gl_.bindTexture(kTexture2D, texture);
        gl_.texParameteri(kTexture2D, kTextureMinFilter, filtering);
        gl_.texParameteri(kTexture2D, kTextureMagFilter, filtering);
    }

    // A target width x height (made again when its size changes); false
    // when the framebuffer is not complete.
    bool prepare(Target& t, int width, int height, GLint filtering) {
        if (t.width != width || t.height != height) {
            release(t);
            gl_.genTextures(1, &t.texture);
            gl_.bindTexture(kTexture2D, t.texture);
            gl_.texImage2D(kTexture2D, 0, static_cast<GLint>(kRgba), width, height, 0, kRgba, kUnsignedByte,
                           nullptr);
            gl_.texParameteri(kTexture2D, kTextureWrapS, kClampToEdge);
            gl_.texParameteri(kTexture2D, kTextureWrapT, kClampToEdge);
            gl_.genFramebuffers(1, &t.framebuffer);
            gl_.bindFramebuffer(kFramebuffer, t.framebuffer);
            gl_.framebufferTexture2D(kFramebuffer, kColorAttachment0, kTexture2D, t.texture, 0);
            t.width = width;
            t.height = height;
        }
        setFiltering(t.texture, filtering);
        gl_.bindFramebuffer(kFramebuffer, t.framebuffer);
        return gl_.checkFramebufferStatus(kFramebuffer) == kFramebufferComplete;
    }

    void release(Target& t) {
        if (t.framebuffer != 0) {
            gl_.deleteFramebuffers(1, &t.framebuffer);
        }
        if (t.texture != 0) {
            gl_.deleteTextures(1, &t.texture);
        }
        t = Target{};
    }

    // Draws texture with program over the viewport (x, y from the bottom).
    void pass(const Program& program, GLuint texture, GLuint quad, int x, int y, int width, int height) {
        gl_.viewport(x, y, width, height);
        gl_.useProgram(program.id);
        gl_.activeTexture(kTexture0);
        gl_.bindTexture(kTexture2D, texture);
        gl_.uniform1i(program.source, 0);
        gl_.bindBuffer(kArrayBuffer, quad);
        const auto stride = static_cast<GLsizei>(4 * sizeof(GLfloat));
        gl_.vertexAttribPointer(kPositionAttribute, 2, kFloat, 0, stride, nullptr);
        gl_.vertexAttribPointer(kTexCoordAttribute, 2, kFloat, 0, stride,
                                reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
        gl_.enableVertexAttribArray(kPositionAttribute);
        gl_.enableVertexAttribArray(kTexCoordAttribute);
        gl_.drawArrays(kTriangleStrip, 0, 4);
    }

    void sizes(const Program& program, float scaleX, float scaleY, int outputWidth, int outputHeight) {
        gl_.useProgram(program.id);
        if (program.sourceSize >= 0) {
            gl_.uniform2f(program.sourceSize, static_cast<GLfloat>(frameWidth_),
                          static_cast<GLfloat>(frameHeight_));
        }
        if (program.scale >= 0) {
            gl_.uniform2f(program.scale, scaleX, scaleY);
        }
        if (program.outputSize >= 0) {
            gl_.uniform2f(program.outputSize, static_cast<GLfloat>(outputWidth),
                          static_cast<GLfloat>(outputHeight));
        }
    }

    // The frame on framebuffer target, width x height: black, and the
    // picture where the viewport puts it.
    void draw(GLuint target, int width, int height) {
        const gfx::Viewport view = viewport(width, height);
        if (view.picture.empty() || frameWidth_ == 0) {
            gl_.bindFramebuffer(kFramebuffer, target);
            gl_.viewport(0, 0, width, height);
            gl_.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl_.clear(kColorBufferBit);
            return;
        }
        const gfx::Rect& p = view.picture;
        const int bottom = height - p.y - p.h;  // OpenGL counts rows from the bottom

        if (filter_ == gfx::Filter::Xbr) {
            // First pass: the frame xbrScale() times, its edges smoothed.
            const int k = xbrScale(view);
            const bool same = p.w == frameWidth_ * k && p.h == frameHeight_ * k;
            setFiltering(frameTexture_, kNearest);
            if (prepare(xbrPass_, frameWidth_ * k, frameHeight_ * k, same ? kNearest : kLinear)) {
                sizes(xbr_, static_cast<float>(k), static_cast<float>(k), xbrPass_.width, xbrPass_.height);
                pass(xbr_, frameTexture_, quad_, 0, 0, xbrPass_.width, xbrPass_.height);
                clearTarget(target, width, height);
                pass(plain_, xbrPass_.texture, quadFromPass_, p.x, bottom, p.w, p.h);
                return;
            }
            // (No framebuffer: CRISP.)
        }

        clearTarget(target, width, height);
        switch (filter_) {
            case gfx::Filter::Sharp:
                setFiltering(frameTexture_, kNearest);
                pass(plain_, frameTexture_, quad_, p.x, bottom, p.w, p.h);
                break;
            case gfx::Filter::Smooth:
                setFiltering(frameTexture_, kLinear);
                pass(plain_, frameTexture_, quad_, p.x, bottom, p.w, p.h);
                break;
            case gfx::Filter::Crt:
                setFiltering(frameTexture_, kLinear);
                sizes(crt_, 1.0f, 1.0f, p.w, p.h);
                pass(crt_, frameTexture_, quad_, p.x, bottom, p.w, p.h);
                break;
            case gfx::Filter::Crisp:
            case gfx::Filter::Xbr:
                if (view.prescale > 1) {
                    setFiltering(frameTexture_, kLinear);
                    sizes(crisp_, static_cast<float>(view.prescale), static_cast<float>(view.prescale), p.w,
                          p.h);
                    pass(crisp_, frameTexture_, quad_, p.x, bottom, p.w, p.h);
                } else {
                    setFiltering(frameTexture_, view.smooth ? kLinear : kNearest);
                    pass(plain_, frameTexture_, quad_, p.x, bottom, p.w, p.h);
                }
                break;
        }
    }

    void clearTarget(GLuint target, int width, int height) {
        gl_.bindFramebuffer(kFramebuffer, target);
        gl_.viewport(0, 0, width, height);
        gl_.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl_.clear(kColorBufferBit);
    }

    SDL_Window* window_;
    SDL_GLContext context_;
    Gl gl_;
    bool es_;
    std::string version_;
    bool vsync_ = false;
    Program plain_;
    Program crisp_;
    Program xbr_;
    Program crt_;
    GLuint quad_ = 0;
    GLuint quadFromPass_ = 0;
    GLuint frameTexture_ = 0;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
    Target xbrPass_;
    Target capture_;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    bool integer_ = false;
    gfx::Filter filter_ = gfx::Filter::Crisp;
};

}  // namespace

void prepareOpenGl() {
#if defined(__SWITCH__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

std::unique_ptr<Sdl2DisplayBase> createGlDisplay(SDL_Window* window, const DisplayOptions& options,
                                                 std::string& error) {
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        error = SDL_GetError();
        SDL_DestroyWindow(window);
        return nullptr;
    }
    Gl gl;
    const std::string missing = gl.load();
    if (!missing.empty()) {
        error = missing + " is missing";
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        return nullptr;
    }
    int profile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    auto display = std::make_unique<GlDisplay>(window, context, gl, profile == SDL_GL_CONTEXT_PROFILE_ES);
    if (!display->start(error)) {
        return nullptr;  // (the display's end destroys the context and the window)
    }
    display->setScaling(options.integerScaling, options.filter);
    return display;
}

}  // namespace bb::platform::sdl2
