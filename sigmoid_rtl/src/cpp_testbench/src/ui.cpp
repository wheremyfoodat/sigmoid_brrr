#include "ui.hpp"

#include <SDL.h>
#include <fmt/format.h>
#include <glad/gl.h>

#include <cstdlib>

#include "bf16.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

// For accessing private fields of pipeline stage
#define GET_FIELD(stage, field) stage.__PVT__##field
#define CHECKBOX(label, value) ImGui::Checkbox(label, (bool*)&value)

void stepCycles(Sigmoid* top, uint cycles);

// Draw top-level module inputs (rst, data_in, valid_in) and outputs (data_out, valid_out)
void UI::drawTopModule(Sigmoid* top) {
    ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
    ImGui::Begin("Top Module");

    static char dataInStr[128] = "-0.5";
    std::string dataOutStr = fmt::format("{:04X} ({:0.4f})", top->data_out, bf16::toFloat(top->data_out));

    ImGui::InputText("data_in", dataInStr, IM_COUNTOF(dataInStr));
    auto dataIn = bf16::fromString(dataInStr);

    if (dataIn.has_value()) {
        top->data_in = *dataIn;
    }

    // data_out shouldn't be toggleable by the user
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::InputText("data_out", (char*)dataOutStr.c_str(), dataOutStr.size());
    ImGui::PopItemFlag();

    CHECKBOX("rst", top->rst);
    CHECKBOX("valid_in", top->valid_in);

    // valid_out also shouldn't be toggleable
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    CHECKBOX("valid_out", top->valid_out);
    ImGui::PopItemFlag();

    if (ImGui::Button("Step")) {
        stepCycles(top, 1);
    }
    ImGui::End();
}

void UI::drawPipeline(Sigmoid* top) {
    ImGui::SetNextWindowSize(ImVec2(720, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(600, 20), ImGuiCond_FirstUseEver);
    ImGui::Begin("Pipeline");

    // None of the widgets below should be toggleable by the user
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

    UI::drawStage0(top);
    ImGui::Separator();

    UI::drawStage1(top);
    ImGui::Separator();

    UI::drawStage2(top);
    ImGui::Separator();

    UI::drawStage3(top);
    ImGui::Separator();

    UI::drawStage4(top);

    ImGui::PopItemFlag();
    ImGui::End();
}

void UI::drawStage0(Sigmoid* top) {
    auto& stage = top->stage0_out;
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Stage 0");

    const auto& valid = GET_FIELD(stage, valid);
    const auto& is_negative = GET_FIELD(stage, is_negative);
    const auto& x_abs = GET_FIELD(stage, x_abs);
    std::string x_abs_str = fmt::format("{:04X} ({:0.4f})", x_abs, bf16::toFloat(x_abs));

    CHECKBOX("valid##0", valid);
    CHECKBOX("is_negative##0", is_negative);
    ImGui::InputText("x_abs", (char*)x_abs_str.c_str(), x_abs_str.size());
}

void UI::drawStage1(Sigmoid* top) {
    auto& stage = top->stage1_out;
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Stage 1");

    const auto& valid = GET_FIELD(stage, valid);
    const auto& is_negative = GET_FIELD(stage, is_negative);

    const auto& x_offset = GET_FIELD(stage, x_offset);
    const auto& a0 = GET_FIELD(stage, a0);
    const auto& a1 = GET_FIELD(stage, a1);
    const auto& a2 = GET_FIELD(stage, a2);

    std::string x_offset_str = fmt::format("{:04X} ({:0.4f})", x_offset, bf16::toFloat(x_offset));
    std::string a0_str = fmt::format("{:04X} ({:0.4f})", a0, bf16::toFloat(a0));
    std::string a1_str = fmt::format("{:04X} ({:0.4f})", a1, bf16::toFloat(a1));
    std::string a2_str = fmt::format("{:04X} ({:0.4f})", a2, bf16::toFloat(a2));

    CHECKBOX("valid##1", valid);
    CHECKBOX("is_negative##1", is_negative);

    ImGui::InputText("x_offset##1", (char*)x_offset_str.c_str(), x_offset_str.size());
    ImGui::InputText("a0##1", (char*)a0_str.c_str(), a0_str.size());
    ImGui::InputText("a1##1", (char*)a1_str.c_str(), a1_str.size());
    ImGui::InputText("a2##1", (char*)a2_str.c_str(), a2_str.size());
}

void UI::drawStage2(Sigmoid* top) {
    auto& stage = top->stage2_out;
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Stage 2");

    const auto& valid = GET_FIELD(stage, valid);
    const auto& is_negative = GET_FIELD(stage, is_negative);

    const auto& x_squared = GET_FIELD(stage, x_squared);
    const auto& a0 = GET_FIELD(stage, a0);
    const auto& mul_a1_x = GET_FIELD(stage, mul_a1_x);
    const auto& a2 = GET_FIELD(stage, a2);

    std::string x_squared_str = fmt::format("{:04X} ({:0.4f})", x_squared, bf16::toFloat(x_squared));
    std::string a0_str = fmt::format("{:04X} ({:0.4f})", a0, bf16::toFloat(a0));
    std::string mul_a1_x_str = fmt::format("{:04X} ({:0.4f})", mul_a1_x, bf16::toFloat(mul_a1_x));
    std::string a2_str = fmt::format("{:04X} ({:0.4f})", a2, bf16::toFloat(a2));

    CHECKBOX("valid##2", valid);
    CHECKBOX("is_negative##2", is_negative);

    ImGui::InputText("x_squared##2", (char*)x_squared_str.c_str(), x_squared_str.size());
    ImGui::InputText("mul_a1_x##2", (char*)mul_a1_x_str.c_str(), mul_a1_x_str.size());
    ImGui::InputText("a0##2", (char*)a0_str.c_str(), a0_str.size());
    ImGui::InputText("a2##2", (char*)a2_str.c_str(), a2_str.size());
}

void UI::drawStage3(Sigmoid* top) {
    auto& stage = top->stage3_out;
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Stage 3");

    const auto& valid = GET_FIELD(stage, valid);
    const auto& is_negative = GET_FIELD(stage, is_negative);

    const auto& mul_a2_x2 = GET_FIELD(stage, mul_a2_x2);
    const auto& add_a0_a1 = GET_FIELD(stage, add_a0_a1);

    std::string mul_a2_x2_str = fmt::format("{:04X} ({:0.4f})", mul_a2_x2, bf16::toFloat(mul_a2_x2));
    std::string add_a0_a1_str = fmt::format("{:04X} ({:0.4f})", add_a0_a1, bf16::toFloat(add_a0_a1));

    CHECKBOX("valid##3", valid);
    CHECKBOX("is_negative##3", is_negative);

    ImGui::InputText("mul_a2_x2##3", (char*)mul_a2_x2_str.c_str(), mul_a2_x2_str.size());
    ImGui::InputText("add_a0_a1##3", (char*)add_a0_a1_str.c_str(), add_a0_a1_str.size());
}

void UI::drawStage4(Sigmoid* top) {
    auto& stage = top->stage4_out;
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Stage 4");

    const auto& valid = GET_FIELD(stage, valid);
    const auto& result = GET_FIELD(stage, result);

    std::string result_str = fmt::format("{:04X} ({:0.4f})", result, bf16::toFloat(result));

    CHECKBOX("valid##4", valid);
    ImGui::InputText("result##4", (char*)result_str.c_str(), result_str.size());
}

std::pair<SDL_Window*, SDL_GLContext> UI::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        abort();
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glslVersion = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glslVersion = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glslVersion = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glslVersion = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    float mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(
        "Crisp Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)(1280 * mainScale), (int)(800 * mainScale), windowFlags
    );

    if (window == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        abort();
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        abort();
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        printf("OpenGL initialization failed\n");
        abort();
    }

    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.Fonts->AddFontDefaultVector();

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(mainScale);
    style.FontScaleDpi = mainScale;

    // Rounded edges
    style.TabRounding = 8.f;
    style.FrameRounding = 0.f;
    style.GrabRounding = 8.f;
    style.WindowRounding = 8.f;
    style.PopupRounding = 8.f;

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glslVersion);

    return {window, glContext};
}

void UI::deinit(SDL_Window* window, SDL_GLContext glContext) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void UI::startFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void UI::endFrame(SDL_Window* window) {
    constexpr ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();

    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}