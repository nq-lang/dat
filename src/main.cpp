// ═══════════════════════════════════════════════════════════════════════════════
// main.cpp — FILE 4 of 4
// Macro Intelligence Terminal — Application Shell + Entry Point
//
// Consolidated from the prior multi-file layout. Contains:
//   • Application  — owns GLFW window, OpenGL 4.6 context, ImGui context;
//                    composes Section 1 (Globe) / Section 2 (Feed) / Section 3
//                    (Topography) + FilterRail + LocationContextPanel + StatusBar;
//                    drives the 60fps render loop.
//   • main()       — loads .env, loads Secrets, constructs Application, runs it.
//
// Depends on core.hpp, providers.hpp, ui.hpp (all three other files).
//
// ─────────────────────────────────────────────────────────────────────────────
// BUILD INSTRUCTIONS (CMake + vcpkg) — since this project needs a build system
// but you asked for <=4 *source* files, the CMake configuration is embedded
// below as a heredoc comment. Copy the two blocks into CMakeLists.txt and
// vcpkg.json respectively (or run the one-line `cat` command shown) before
// building.
// ─────────────────────────────────────────────────────────────────────────────
/*
=== CMakeLists.txt ===
cmake_minimum_required(VERSION 3.28)
project(MacroTerminal VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

add_executable(macro-terminal src/main.cpp)
target_compile_features(macro-terminal PUBLIC cxx_std_23)
target_include_directories(macro-terminal PRIVATE src)
target_link_libraries(macro-terminal PRIVATE
    glfw glad::glad imgui::imgui nlohmann_json::nlohmann_json
    Eigen3::Eigen CURL::libcurl)
if (UNIX AND NOT APPLE)
    target_link_libraries(macro-terminal PRIVATE dl pthread)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(macro-terminal PRIVATE -fcoroutines)
endif()

=== vcpkg.json ===
{
  "name": "macro-terminal",
  "version": "0.4.0",
  "dependencies": [
    "glfw3", "glad",
    { "name": "imgui", "features": ["glfw-binding", "opengl3-binding", "docking-experimental"] },
    "nlohmann-json", "eigen3", "curl"
  ]
}

=== Build ===
  git clone https://github.com/microsoft/vcpkg.git && export VCPKG_ROOT=$(pwd)/vcpkg
  ./vcpkg/bootstrap-vcpkg.sh
  cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
  cmake --build build --parallel
  cp .env.example .env   # fill in API keys
  ./build/bin/macro-terminal
*/
// ═══════════════════════════════════════════════════════════════════════════════

#include "core.hpp"
#include "providers.hpp"
#include "ui.hpp"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <cstring>

namespace macro {

struct AppError { std::string message; };

class Application {
public:
    struct Config {
        int window_width{1920}, window_height{1080};
        const char* window_title{"MACRO INTELLIGENCE TERMINAL  //  INTERNAL USE ONLY"};
        bool fullscreen{false};
        float s1{0.37f}, s2{0.34f}, s3{0.29f};
    };

    [[nodiscard]] static std::expected<Application, AppError> create(const Secrets& sec, Config cfg = {}) {
        Application a(sec, cfg);
        if (auto e = a.init(); !e) return MACRO_UNEXPECTED(e.error());
        return a;
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = default;
    Application& operator=(Application&&) = default;
    ~Application() { shutdown(); }

    void run() {
        engine_->start();
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            tick();
            render_frame();
            glfwSwapBuffers(window_);
        }
        engine_->stop();
    }

private:
    Config cfg_;
    GLFWwindow* window_{nullptr};
    AppStateBus bus_;
    SourceHealth health_;
    RecordQueue rq_;
    Secrets sec_;
    AppStateBus::Token ctx_tok_{};

    std::unique_ptr<GlobeLayer> globe_;
    std::unique_ptr<TablesLayer> tables_;
    std::unique_ptr<TopographyLayer> topo_;
    std::unique_ptr<FilterRail> rail_;
    std::unique_ptr<LocationContextPanel> ctx_panel_;
    std::unique_ptr<StatusBar> status_bar_;
    std::unique_ptr<ProviderEngine> engine_;

    Application(const Secrets& sec, Config cfg) : cfg_(cfg), sec_(sec) {}

    std::expected<void, AppError> init() {
        if (!glfwInit()) return MACRO_UNEXPECTED(AppError{"glfwInit failed"});
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        GLFWmonitor* mon = cfg_.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        window_ = glfwCreateWindow(cfg_.window_width, cfg_.window_height, cfg_.window_title, mon, nullptr);
        if (!window_) { glfwTerminate(); return MACRO_UNEXPECTED(AppError{"glfwCreateWindow failed"}); }
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            return MACRO_UNEXPECTED(AppError{"gladLoadGLLoader failed"});
        glEnable(GL_MULTISAMPLE);
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "[App] OpenGL %s / %s",
                (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
            std::puts(buf);
        }
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "macro_terminal_layout.ini";
        Theme::apply_style();
        load_fonts(io);
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        rail_ = std::make_unique<FilterRail>();
        ctx_panel_ = std::make_unique<LocationContextPanel>();
        status_bar_ = std::make_unique<StatusBar>(health_);
        globe_ = std::make_unique<GlobeLayer>(bus_);
        tables_ = std::make_unique<TablesLayer>(bus_, sec_);
        topo_ = std::make_unique<TopographyLayer>(bus_, sec_.anthropic_api_key);
        ctx_tok_ = bus_.subscribe([this](const GeoSelectionContext& c) { ctx_panel_->update_context(c); });
        engine_ = std::make_unique<ProviderEngine>(sec_, rq_);
        std::puts("[App] initialisation complete");
        return {};
    }

    void tick() {
        bus_.dispatch_pending();
        tables_->tick();
        for (auto& rec : rq_.drain(400)) {
            tables_->ingest(rec);
            topo_->ingest(rec);
            ctx_panel_->ingest_record(rec);
            globe_->ingest_record(rec);
        }
        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last > std::chrono::seconds{2}) {
            health_.total.store(engine_->total());
            health_.ok.store(engine_->healthy());
            health_.err.store(engine_->failed());
            last = now;
        }
    }

    void render_frame() {
        int ww, wh; glfwGetFramebufferSize(window_, &ww, &wh);
        glViewport(0, 0, ww, wh);
        glClearColor(0.039f, 0.055f, 0.078f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            constexpr ImGuiWindowFlags HF = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToDisplayFrontOnFocus |
                ImGuiWindowFlags_NoSavedSettings;
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::Begin("##Host", nullptr, HF);
            ImGui::DockSpace(ImGui::GetID("##Dock"), {0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::End();
            ImGui::PopStyleVar();
        }
        float fw = (float)ww, fh = (float)wh;
        float rw = rail_->visible_width(), cxw = ctx_panel_->visible_width();
        float cw = fw - rw - cxw;
        float bh = Theme::STATUS_BAR_HEIGHT, av = fh - bh;
        float s1h = av * cfg_.s1, s2h = av * cfg_.s2, s3h = av * cfg_.s3;

        {
            constexpr ImGuiWindowFlags GF = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
            ImGui::SetNextWindowPos({rw, 0});
            ImGui::SetNextWindowSize({cw, s1h});
            ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_PRIMARY);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            if (ImGui::Begin("##S1", nullptr, GF)) globe_->render(cw, s1h);
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        tables_->render(rw, s1h, cw, s2h);
        topo_->render(rw, s1h + s2h, cw, s3h);
        rail_->render(fh);
        ctx_panel_->render(fh);
        status_bar_->render(fw);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void load_fonts(ImGuiIO& io) {
        bool ok = false;
        for (const char* p : {"data/fonts/JetBrainsMono-Regular.ttf", "data/fonts/IBMPlexMono-Regular.ttf"})
            if (io.Fonts->AddFontFromFileTTF(p, 13.0f)) { ok = true; break; }
        if (!ok) io.Fonts->AddFontDefault();
        for (const char* p : {"data/fonts/IBMPlexSansCondensed-Regular.ttf", "data/fonts/Inter-Regular.ttf"})
            if (io.Fonts->AddFontFromFileTTF(p, 11.0f)) break;
        io.Fonts->Build();
    }

    void shutdown() {
        if (ctx_tok_) bus_.unsubscribe(ctx_tok_);
        if (window_) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
    }
};

} // namespace macro

// ─────────────────────────────── main() ────────────────────────────────────────
static void try_load_dotenv(const char* path = ".env") {
    std::FILE* f = std::fopen(path, "r");
    if (!f) return;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        for (char* p = line; *p; ++p) if (*p == '\n' || *p == '\r') { *p = '\0'; break; }
        if (line[0] == '#' || line[0] == '\0') continue;
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;
        if (!std::getenv(key)) ::setenv(key, val, 0);
    }
    std::fclose(f);
    MACRO_LOG(".env loaded from %s", path);
}

int main(int argc, char** argv) {
    std::puts("===============================================================");
    std::puts("  MACRO INTELLIGENCE TERMINAL  v0.4  (C++23, 4-file build)");
    std::puts("  Internal Use Only - Read-Only Analytics");
    std::puts("===============================================================");

    macro::Application::Config cfg;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fullscreen") == 0) cfg.fullscreen = true;
        if (std::strcmp(argv[i], "--env") == 0 && i + 1 < argc) try_load_dotenv(argv[++i]);
    }
    try_load_dotenv(".env");

    auto sec_result = macro::load_secrets();
    if (!sec_result) {
        auto& err = sec_result.error();
        std::puts("[main] -- MISSING REQUIRED API KEYS --------------------------");
        for (auto& v : err.missing_vars) MACRO_LOG("  export %s=<your-key>", v.c_str());
        std::puts("[main] Terminal starting in degraded mode (providers disabled).");
        macro::Secrets empty{};
        auto app = macro::Application::create(empty, cfg);
        if (!app) { MACRO_LOG("[main] FATAL: %s", app.error().message.c_str()); return EXIT_FAILURE; }
        app->run();
        return EXIT_SUCCESS;
    }

    auto app = macro::Application::create(*sec_result, cfg);
    if (!app) { MACRO_LOG("[main] FATAL: %s", app.error().message.c_str()); return EXIT_FAILURE; }

    std::puts("[main] Application initialized - entering render loop");
    app->run();
    std::puts("[main] Clean shutdown complete");
    return EXIT_SUCCESS;
}
