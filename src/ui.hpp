#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// ui.hpp — FILE 3 of 4
// Macro Intelligence Terminal — UI / Rendering Layer
//
// Consolidated from the prior multi-file layout. Contains:
//   • Theme              — §2 institutional muted color palette + ImGui style
//   • StatusBar           — bottom persistent bar (sources/ok/err/UTC/severity)
//   • FilterRail          — left 220px domain filter panel
//   • LocationContextPanel— right 300px location/macro/provider tabs
//   • GMSIComputer        — Global Macro Stress Index (feeds globe heat overlay)
//   • GlobeLayer          — SECTION 1: globe + overlay toggles + alert stream
//   • FeedModule          — single narrative feed card renderer (Section 2)
//   • TablesLayer         — SECTION 2: 7 geo-scoped debounced narrative feeds
//   • FactorModel         — Eigen OLS regime scoring + TerrainGrid
//   • LLMRationaleService — Anthropic claude-sonnet-4-6 async rationale pipeline
//   • TopographyLayer     — SECTION 3: 3D terrain + regime tables + search
//
// Depends on core.hpp and providers.hpp (included by the consuming .cpp
// before this file).
// ═══════════════════════════════════════════════════════════════════════════════
#include "core.hpp"
#include "providers.hpp"
#include <imgui.h>
#include <glad/glad.h>
#include <Eigen/Dense>
#include <nlohmann/json.hpp>
#include <deque>
#include <unordered_map>
#include <cmath>

namespace macro {

// ═══════════════════════════════ THEME ═════════════════════════════════════════
namespace Theme {
constexpr ImVec4 BG_PRIMARY   {0.039f,0.055f,0.078f,1.0f};
constexpr ImVec4 BG_SECONDARY {0.051f,0.067f,0.090f,1.0f};
constexpr ImVec4 BG_PANEL     {0.063f,0.082f,0.110f,1.0f};
constexpr ImVec4 BG_ELEVATED  {0.075f,0.098f,0.130f,1.0f};
constexpr ImVec4 BORDER_SUBTLE {0.18f,0.22f,0.28f,0.45f};
constexpr ImVec4 ACCENT_CYAN      {0.18f,0.83f,1.00f,1.0f};
constexpr ImVec4 ACCENT_CYAN_DIM  {0.18f,0.83f,1.00f,0.20f};
constexpr ImVec4 ACCENT_BLUE_SOLID{0.23f,0.49f,0.65f,1.0f};
constexpr ImVec4 SEV_INFORMATIONAL{0.55f,0.62f,0.70f,1.0f};
constexpr ImVec4 SEV_LOW          {0.45f,0.58f,0.52f,1.0f};
constexpr ImVec4 SEV_ELEVATED     {0.70f,0.68f,0.42f,1.0f};
constexpr ImVec4 SEV_HIGH         {0.78f,0.52f,0.32f,1.0f};
constexpr ImVec4 SEV_CRITICAL     {0.71f,0.35f,0.29f,1.0f};
constexpr ImVec4 SEV_SYSTEMIC     {0.82f,0.22f,0.18f,1.0f};
constexpr ImVec4 DIR_BULLISH{0.30f,0.49f,0.47f,1.0f};
constexpr ImVec4 DIR_BEARISH{0.71f,0.35f,0.29f,1.0f};
constexpr ImVec4 DIR_NEUTRAL{0.50f,0.55f,0.60f,1.0f};
constexpr ImVec4 TEXT_PRIMARY  {0.86f,0.88f,0.90f,1.0f};
constexpr ImVec4 TEXT_SECONDARY{0.55f,0.60f,0.65f,1.0f};
constexpr ImVec4 TEXT_MUTED    {0.35f,0.38f,0.42f,1.0f};
constexpr ImVec4 TEXT_STALE    {0.71f,0.35f,0.29f,1.0f};
constexpr ImVec4 CONVICTION_HIGH   = DIR_BULLISH;
constexpr ImVec4 CONVICTION_MEDIUM = SEV_ELEVATED;
constexpr ImVec4 CONVICTION_LOW    = SEV_INFORMATIONAL;
constexpr float PADDING_OUTER=12.0f, PADDING_INNER=8.0f, ROUNDING_PANEL=4.0f, ROUNDING_CHIP=3.0f;
constexpr float BORDER_WIDTH=1.0f, STATUS_BAR_HEIGHT=24.0f, FILTER_RAIL_WIDTH=220.0f, CONTEXT_PANEL_WIDTH=300.0f;

inline void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]=BG_PRIMARY; c[ImGuiCol_ChildBg]=BG_PANEL; c[ImGuiCol_PopupBg]=BG_SECONDARY;
    c[ImGuiCol_Border]=BORDER_SUBTLE; c[ImGuiCol_BorderShadow]={0,0,0,0};
    c[ImGuiCol_FrameBg]=BG_ELEVATED; c[ImGuiCol_FrameBgHovered]={0.10f,0.14f,0.18f,1.0f}; c[ImGuiCol_FrameBgActive]={0.13f,0.18f,0.24f,1.0f};
    c[ImGuiCol_TitleBg]=BG_SECONDARY; c[ImGuiCol_TitleBgActive]={0.08f,0.11f,0.15f,1.0f}; c[ImGuiCol_TitleBgCollapsed]=BG_PRIMARY;
    c[ImGuiCol_MenuBarBg]=BG_SECONDARY; c[ImGuiCol_ScrollbarBg]=BG_PRIMARY; c[ImGuiCol_ScrollbarGrab]=BORDER_SUBTLE;
    c[ImGuiCol_ScrollbarGrabHovered]={0.25f,0.30f,0.36f,1.0f}; c[ImGuiCol_ScrollbarGrabActive]=ACCENT_BLUE_SOLID;
    c[ImGuiCol_CheckMark]=ACCENT_CYAN; c[ImGuiCol_SliderGrab]=ACCENT_BLUE_SOLID; c[ImGuiCol_SliderGrabActive]=ACCENT_CYAN;
    c[ImGuiCol_Button]=BG_ELEVATED; c[ImGuiCol_ButtonHovered]={0.15f,0.21f,0.28f,1.0f}; c[ImGuiCol_ButtonActive]=ACCENT_BLUE_SOLID;
    c[ImGuiCol_Header]={0.12f,0.17f,0.22f,1.0f}; c[ImGuiCol_HeaderHovered]={0.16f,0.22f,0.29f,1.0f}; c[ImGuiCol_HeaderActive]=ACCENT_BLUE_SOLID;
    c[ImGuiCol_Separator]=BORDER_SUBTLE; c[ImGuiCol_SeparatorHovered]=ACCENT_CYAN_DIM; c[ImGuiCol_SeparatorActive]=ACCENT_CYAN;
    c[ImGuiCol_Tab]=BG_SECONDARY; c[ImGuiCol_TabHovered]={0.14f,0.19f,0.25f,1.0f}; c[ImGuiCol_TabActive]=BG_ELEVATED;
    c[ImGuiCol_TabUnfocused]=BG_SECONDARY; c[ImGuiCol_TabUnfocusedActive]=BG_PANEL;
    c[ImGuiCol_DockingPreview]=ACCENT_CYAN_DIM; c[ImGuiCol_DockingEmptyBg]=BG_PRIMARY;
    c[ImGuiCol_TableHeaderBg]=BG_SECONDARY; c[ImGuiCol_TableBorderLight]=BORDER_SUBTLE;
    c[ImGuiCol_TableBorderStrong]={0.22f,0.27f,0.33f,0.80f}; c[ImGuiCol_TableRowBg]={0,0,0,0}; c[ImGuiCol_TableRowBgAlt]={0.06f,0.08f,0.11f,0.50f};
    c[ImGuiCol_TextSelectedBg]=ACCENT_CYAN_DIM; c[ImGuiCol_DragDropTarget]=ACCENT_CYAN; c[ImGuiCol_NavHighlight]=ACCENT_CYAN;
    c[ImGuiCol_Text]=TEXT_PRIMARY; c[ImGuiCol_TextDisabled]=TEXT_MUTED;
    c[ImGuiCol_PlotLines]=ACCENT_CYAN; c[ImGuiCol_PlotLinesHovered]=TEXT_PRIMARY;
    c[ImGuiCol_PlotHistogram]=ACCENT_BLUE_SOLID; c[ImGuiCol_PlotHistogramHovered]=ACCENT_CYAN;
    c[ImGuiCol_ResizeGrip]={0,0,0,0}; c[ImGuiCol_ResizeGripHovered]=ACCENT_CYAN_DIM; c[ImGuiCol_ResizeGripActive]=ACCENT_CYAN;
    s.WindowRounding=ROUNDING_PANEL; s.ChildRounding=ROUNDING_PANEL; s.FrameRounding=ROUNDING_CHIP; s.PopupRounding=ROUNDING_PANEL;
    s.ScrollbarRounding=2.0f; s.GrabRounding=2.0f; s.TabRounding=2.0f;
    s.WindowBorderSize=BORDER_WIDTH; s.ChildBorderSize=BORDER_WIDTH; s.FrameBorderSize=0.0f; s.PopupBorderSize=BORDER_WIDTH;
    s.WindowPadding={PADDING_OUTER,PADDING_OUTER}; s.FramePadding={PADDING_INNER,PADDING_INNER/2.0f};
    s.ItemSpacing={8.0f,4.0f}; s.ItemInnerSpacing={4.0f,4.0f}; s.CellPadding={PADDING_INNER,3.0f};
    s.IndentSpacing=16.0f; s.ScrollbarSize=10.0f; s.GrabMinSize=6.0f;
}

inline const ImVec4& severity_color(int level) {
    static constexpr ImVec4 pal[6]={SEV_INFORMATIONAL,SEV_LOW,SEV_ELEVATED,SEV_HIGH,SEV_CRITICAL,SEV_SYSTEMIC};
    if (level<0) return pal[0]; if (level>5) return pal[5]; return pal[(std::size_t)level];
}
inline const ImVec4& direction_color(int dir) {
    if (dir>0) return DIR_BULLISH; if (dir<0) return DIR_BEARISH; return DIR_NEUTRAL;
}
} // namespace Theme

// ═══════════════════════════════ STATUS BAR ═══════════════════════════════════
struct SourceHealth { std::atomic<int> total{0}, ok{0}, err{0}; };

class StatusBar {
public:
    explicit StatusBar(SourceHealth& h) : h_(h) {}
    void render(float vw) {
        const float y = ImGui::GetIO().DisplaySize.y - Theme::STATUS_BAR_HEIGHT;
        ImGui::SetNextWindowPos({0.0f, y});
        ImGui::SetNextWindowSize({vw, Theme::STATUS_BAR_HEIGHT});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_SECONDARY);
        ImGui::PushStyleColor(ImGuiCol_Border, Theme::BORDER_SUBTLE);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f,3.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
            ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##SB", nullptr, F)) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()+2.0f);
            ImGui::TextColored(Theme::TEXT_SECONDARY, "%d sources", h_.total.load());
            ImGui::SameLine(0,12);
            ImGui::TextColored(Theme::DIR_BULLISH, "\xe2\x97\x8f %d ok", h_.ok.load());
            ImGui::SameLine(0,12);
            int e = h_.err.load();
            ImGui::TextColored(e>0?Theme::SEV_CRITICAL:Theme::TEXT_MUTED, "\xe2\x97\x8f %d err", e);
            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            std::tm gm{}; gmtime_r(&tt,&gm);
            char tbuf[20]; std::strftime(tbuf,sizeof(tbuf),"%H:%M:%S UTC",&gm);
            float tw = ImGui::CalcTextSize(tbuf).x;
            ImGui::SameLine(vw/2.0f - tw/2.0f);
            ImGui::TextColored(Theme::TEXT_MUTED, "%s", tbuf);
            struct Chip { const char* lbl; const ImVec4* col; };
            static const Chip CHIPS[] = {
                {"INFO",&Theme::SEV_INFORMATIONAL},{"LOW",&Theme::SEV_LOW},{"ELEVATED",&Theme::SEV_ELEVATED},
                {"HIGH",&Theme::SEV_HIGH},{"CRITICAL",&Theme::SEV_CRITICAL},{"SYSTEMIC",&Theme::SEV_SYSTEMIC},
            };
            float rx = vw - 8.0f;
            for (int i=5;i>=0;--i) {
                float cw = ImGui::CalcTextSize(CHIPS[i].lbl).x + 18.0f;
                rx -= cw;
                ImGui::SameLine(rx);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                float th = ImGui::GetTextLineHeight();
                dl->AddRectFilled({p.x,p.y-1},{p.x+cw-4,p.y+th+1},
                    ImGui::ColorConvertFloat4ToU32({CHIPS[i].col->x,CHIPS[i].col->y,CHIPS[i].col->z,0.18f}), Theme::ROUNDING_CHIP);
                ImGui::TextColored(*CHIPS[i].col, "\xe2\x97\x8f %s", CHIPS[i].lbl);
                rx -= 6.0f;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(2);
    }
private:
    SourceHealth& h_;
};

// ═══════════════════════════════ FILTER RAIL ═══════════════════════════════════
enum DomainBit : uint32_t {
    DOMAIN_MACRO=1u<<0, DOMAIN_MICRO=1u<<1, DOMAIN_GEOPOLITICS=1u<<2,
    DOMAIN_CENTRAL_BANK=1u<<3, DOMAIN_MONETARY=1u<<4, DOMAIN_NEWS=1u<<5, DOMAIN_MILITARY=1u<<6,
    OVERLAY_BORDERS=1u<<7, OVERLAY_HEAT=1u<<8, OVERLAY_SATELLITE=1u<<9, OVERLAY_GRID=1u<<10,
    DOMAIN_ALL=0x7FFu,
};
struct DomainEntry { std::string_view label; DomainBit bit; int count{0}; };

class FilterRail {
public:
    FilterRail() : mask_(DOMAIN_ALL) {}
    [[nodiscard]] uint32_t enabled_mask() const noexcept { return mask_.load(std::memory_order_relaxed); }
    void set_count(DomainBit b, int c) {
        for (auto& d : domains_) if (d.bit==b) { d.count=c; return; }
        for (auto& d : overlays_) if (d.bit==b) { d.count=c; return; }
    }
    void render(float vh) {
        if (collapsed_) { render_collapsed(); return; }
        ImGui::SetNextWindowPos({0.0f,0.0f});
        ImGui::SetNextWindowSize({Theme::FILTER_RAIL_WIDTH, vh-Theme::STATUS_BAR_HEIGHT});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_SECONDARY);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f,10.0f});
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##FilterRail", nullptr, F)) {
            ImGui::TextColored(Theme::TEXT_MUTED, "FILTERS");
            ImGui::SameLine(Theme::FILTER_RAIL_WIDTH-36.0f);
            if (ImGui::SmallButton("<")) collapsed_ = true;
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::BG_ELEVATED);
            ImGui::InputText("##srch", sbuf_, sizeof(sbuf_));
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::TextColored(Theme::TEXT_MUTED, "FEED DOMAINS");
            render_tree(domains_, 7);
            ImGui::Separator(); ImGui::Spacing();
            ImGui::TextColored(Theme::TEXT_MUTED, "GLOBE OVERLAYS");
            render_tree(overlays_, 4);
            ImGui::Separator(); ImGui::Spacing();
            ImGui::TextColored(Theme::TEXT_MUTED, "BOUNDARY");
            ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "  DE JURE (UN)");
        }
        ImGui::End();
        ImGui::PopStyleVar(); ImGui::PopStyleColor();
    }
    [[nodiscard]] float visible_width() const noexcept { return collapsed_?24.0f:Theme::FILTER_RAIL_WIDTH; }
private:
    std::atomic<uint32_t> mask_;
    bool collapsed_{false};
    char sbuf_[64]{};
    DomainEntry domains_[7] = {
        {"Macro Developments", DOMAIN_MACRO}, {"Micro Developments", DOMAIN_MICRO},
        {"Geopolitical Tensions", DOMAIN_GEOPOLITICS}, {"Central Bank Updates", DOMAIN_CENTRAL_BANK},
        {"Monetary Policy", DOMAIN_MONETARY}, {"Global / Regional", DOMAIN_NEWS}, {"Military & War", DOMAIN_MILITARY},
    };
    DomainEntry overlays_[4] = {
        {"Admin Boundaries", OVERLAY_BORDERS}, {"GMSI Heat Overlay", OVERLAY_HEAT},
        {"Satellite Imagery", OVERLAY_SATELLITE}, {"Admin Gridlines", OVERLAY_GRID},
    };
    void render_tree(DomainEntry* e, int n) {
        uint32_t mask = mask_.load(std::memory_order_relaxed);
        for (int i=0;i<n;++i) {
            auto& d = e[i];
            if (sbuf_[0]) {
                char lo[64]{}, sl[64]{};
                for (int k=0;k<63&&d.label[k];++k) lo[k]=(char)std::tolower(d.label[k]);
                for (int k=0;k<63&&sbuf_[k];++k) sl[k]=(char)std::tolower(sbuf_[k]);
                if (!std::strstr(lo,sl)) continue;
            }
            bool on = (mask & d.bit) != 0;
            ImGui::PushID(i + (int)d.bit);
            if (ImGui::Checkbox("##cb",&on)) { if (on) mask|=d.bit; else mask&=~d.bit; mask_.store(mask,std::memory_order_relaxed); }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::TextColored(on?Theme::TEXT_PRIMARY:Theme::TEXT_MUTED, "%s", d.label.data());
            if (d.count>0) { ImGui::SameLine(); ImGui::TextColored(Theme::ACCENT_CYAN_DIM," %d",d.count); }
        }
    }
    void render_collapsed() {
        ImGui::SetNextWindowPos({0.0f,0.0f});
        ImGui::SetNextWindowSize({24.0f,80.0f});
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_SECONDARY);
        if (ImGui::Begin("##FRC", nullptr, F)) { ImGui::SetCursorPosY(30.0f); if (ImGui::SmallButton(">")) collapsed_=false; }
        ImGui::End();
        ImGui::PopStyleColor();
    }
};

// ═══════════════════════════════ LOCATION CONTEXT PANEL ═══════════════════════
struct LocStat { std::string label, value; };

class LocationContextPanel {
public:
    void update_context(const GeoSelectionContext& ctx) { ctx_ = ctx; stats_ = lookup(ctx); }
    void ingest_record(const NormalizedRecord& rec) {
        if (rec.geo.country_iso2 && *rec.geo.country_iso2 == ctx_.country_iso2)
            if ((rec.domain=="central_bank"||rec.domain=="monetary_policy") && !rec.headline.empty())
                live_rate_ = rec.headline.substr(0,28);
    }
    void render(float vp_h) {
        if (collapsed_) { render_collapsed(); return; }
        float px = ImGui::GetIO().DisplaySize.x - Theme::CONTEXT_PANEL_WIDTH;
        ImGui::SetNextWindowPos({px,0.0f});
        ImGui::SetNextWindowSize({Theme::CONTEXT_PANEL_WIDTH, vp_h-Theme::STATUS_BAR_HEIGHT});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_SECONDARY);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f,10.0f});
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##CtxPnl", nullptr, F)) {
            ImGui::TextColored(Theme::TEXT_SECONDARY, "CONTEXT");
            ImGui::SameLine(Theme::CONTEXT_PANEL_WIDTH-30.0f);
            if (ImGui::SmallButton(">")) collapsed_=true;
            if (ImGui::BeginTabBar("##CtxTabs")) {
                if (ImGui::BeginTabItem("LOCATION")) { tab_location(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("MACRO SNAP")) { tab_macro(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("PROVIDERS")) { tab_providers(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(); ImGui::PopStyleColor();
    }
    [[nodiscard]] float visible_width() const noexcept { return collapsed_?20.0f:Theme::CONTEXT_PANEL_WIDTH; }

    int prov_total{0}, prov_ok{0}, prov_err{0};
    struct ProvStatus { std::string name; bool ok{true}; std::string note; };
    std::vector<ProvStatus> provider_statuses;

private:
    GeoSelectionContext ctx_;
    std::vector<LocStat> stats_;
    std::string live_rate_;
    bool collapsed_{false};

    void tab_location() {
        auto pill = [](const char* lbl) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.23f,0.49f,0.65f,0.22f});
            ImGui::SmallButton(lbl); ImGui::PopStyleColor(); ImGui::SameLine(0,4);
        };
        if (ctx_.resolution==GeoResolution::World) pill("GLOBAL");
        if (!ctx_.continent.empty()) pill(ctx_.continent.c_str());
        if (ctx_.is_g7) pill("G7"); if (ctx_.is_g20) pill("G20");
        if (ctx_.is_eurozone) pill("EUROZONE"); if (ctx_.is_em) pill("EM"); if (ctx_.is_nato) pill("NATO");
        ImGui::NewLine(); ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_PRIMARY, "%s", ctx_.selected_name().c_str());
        ImGui::TextColored(Theme::TEXT_MUTED, "%s \xc2\xb7 Tier %d",
            ctx_.country_iso2.empty()?"\xe2\x80\x94":ctx_.country_iso2.c_str(), fetch_tier(ctx_.resolution));
        ImGui::Separator(); ImGui::Spacing();
        float cw = (Theme::CONTEXT_PANEL_WIDTH-28.0f)/2.0f;
        auto card = [&](const char* lbl, const char* val) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_ELEVATED);
            ImGui::BeginChild(lbl, {cw,46.0f}, true);
            ImGui::TextColored(Theme::TEXT_MUTED, "%s", lbl);
            ImGui::TextColored(Theme::TEXT_PRIMARY, "%s", val);
            ImGui::EndChild(); ImGui::PopStyleColor();
        };
        for (std::size_t i=0;i+1<stats_.size();i+=2) {
            card(stats_[i].label.c_str(), stats_[i].value.c_str());
            ImGui::SameLine(0,6);
            card(stats_[i+1].label.c_str(), stats_[i+1].value.c_str());
        }
        if (stats_.size()%2==1) { auto& s=stats_.back(); card(s.label.c_str(), s.value.c_str()); }
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "BREADCRUMB");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+Theme::CONTEXT_PANEL_WIDTH-20.0f);
        ImGui::TextColored(Theme::TEXT_SECONDARY, "%s", ctx_.breadcrumb().c_str());
        ImGui::PopTextWrapPos();
    }
    void tab_macro() {
        ImGui::TextColored(Theme::TEXT_MUTED, "MACRO SNAPSHOT"); ImGui::Spacing();
        auto row = [](const char* lbl, const char* val, const ImVec4& c) {
            ImGui::TextColored(Theme::TEXT_SECONDARY, "%-20s", lbl);
            ImGui::SameLine(160.0f); ImGui::TextColored(c, "%s", val);
        };
        row("Policy Rate", live_rate_.empty()?"\xe2\x80\x94":live_rate_.c_str(), Theme::TEXT_PRIMARY);
        row("GMSI Score", "\xe2\x80\x94", Theme::SEV_ELEVATED);
        row("VIX", "\xe2\x80\x94", Theme::TEXT_PRIMARY);
        row("US 10Y Real", "\xe2\x80\x94", Theme::TEXT_PRIMARY);
        row("DXY", "\xe2\x80\x94", Theme::TEXT_PRIMARY);
        ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "(Populates as providers ingest)");
    }
    void tab_providers() {
        ImGui::TextColored(Theme::TEXT_MUTED, "PROVIDER HEALTH");
        ImGui::Text("%d total  ", prov_total);
        ImGui::SameLine(); ImGui::TextColored(Theme::DIR_BULLISH, "%d ok", prov_ok);
        ImGui::SameLine(); ImGui::TextColored(Theme::SEV_CRITICAL, " %d err", prov_err);
        ImGui::Separator();
        for (auto& p : provider_statuses) {
            ImGui::TextColored(p.ok?Theme::DIR_BULLISH:Theme::SEV_CRITICAL, "\xe2\x97\x8f"); ImGui::SameLine(0,6);
            ImGui::TextColored(Theme::TEXT_SECONDARY, "%-18.18s", p.name.c_str());
            if (!p.note.empty()) { ImGui::SameLine(); ImGui::TextColored(Theme::TEXT_MUTED, "%s", p.note.c_str()); }
        }
    }
    static std::vector<LocStat> lookup(const GeoSelectionContext& ctx) {
        static const std::unordered_map<std::string, std::vector<LocStat>> DB = {
            {"US", {{"Policy Rate","5.25-5.50%"},{"5Y CDS","22bps"},{"Mkt Cap","$46.2T"},{"Exch Vol","$350B/d"}}},
            {"GB", {{"Policy Rate","5.25%"},{"5Y CDS","31bps"},{"Mkt Cap","$3.1T"},{"Exch Vol","$28B/d"}}},
            {"DE", {{"Policy Rate","3.75%"},{"5Y CDS","29bps"},{"Mkt Cap","$2.3T"},{"Exch Vol","$18B/d"}}},
            {"JP", {{"Policy Rate","-0.10%"},{"5Y CDS","26bps"},{"Mkt Cap","$5.8T"},{"Exch Vol","$32B/d"}}},
            {"CN", {{"Policy Rate","3.45%"},{"5Y CDS","75bps"},{"Mkt Cap","$9.8T"},{"Exch Vol","$42B/d"}}},
        };
        if (auto it = DB.find(ctx.country_iso2); it != DB.end()) return it->second;
        return {{"Resolution", std::to_string((int)ctx.resolution)}, {"Tier", std::to_string(fetch_tier(ctx.resolution))}};
    }
    void render_collapsed() {
        float px = ImGui::GetIO().DisplaySize.x - 20.0f;
        ImGui::SetNextWindowPos({px,0.0f});
        ImGui::SetNextWindowSize({20.0f,60.0f});
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_SECONDARY);
        if (ImGui::Begin("##CtxC", nullptr, F)) { ImGui::SetCursorPosY(24.0f); if (ImGui::SmallButton("<")) collapsed_=false; }
        ImGui::End();
        ImGui::PopStyleColor();
    }
};

// ═══════════════════════════════ GMSI COMPUTER ═════════════════════════════════
struct GMSIScore { float score{0},eq_vol_z{0},cds_z{0},fx_vol_z{0},geo_risk{0},news_vel{0}; };
struct GMSIWeights { float equity_vol{0.25f},cds_spread{0.25f},fx_vol{0.20f},geo_risk{0.15f},news_velocity{0.15f}; };

class GMSIComputer {
public:
    explicit GMSIComputer(GMSIWeights w = {}) : w_(w) { seed_baseline(); }
    void ingest_record(const NormalizedRecord& rec) {
        if (rec.domain!="geopolitics" && rec.domain!="news") return;
        if (!rec.geo.country_iso2) return;
        const auto& iso2 = *rec.geo.country_iso2;
        std::scoped_lock l{m_};
        if (rec.domain=="geopolitics") { geo_acc_[iso2]+=(float)rec.severity; geo_cnt_[iso2]++; }
        else news_vel_[iso2]++;
        dirty_=true;
    }
    void maybe_recompute() { if (!dirty_.load()) return; dirty_=false; std::scoped_lock l{m_}; recompute_locked(); }
    GMSIScore score(const std::string& iso2) const {
        std::scoped_lock l{read_m_};
        auto it = scores_.find(iso2);
        return (it!=scores_.end()) ? it->second : GMSIScore{};
    }
private:
    GMSIWeights w_;
    std::atomic<bool> dirty_{false};
    mutable std::mutex m_, read_m_;
    std::unordered_map<std::string,float> eq_vol_, cds_;
    std::unordered_map<std::string,float> geo_acc_;
    std::unordered_map<std::string,int> geo_cnt_, news_vel_;
    std::unordered_map<std::string,GMSIScore> scores_;

    static float cmean(const std::unordered_map<std::string,float>& m) {
        if (m.empty()) return 0.0f;
        float s=0; for (auto& kv:m) s+=kv.second; return s/(float)m.size();
    }
    static float cstd(const std::unordered_map<std::string,float>& m, float mean) {
        if (m.size()<2) return 1.0f;
        float s=0; for (auto& kv:m) s+=(kv.second-mean)*(kv.second-mean);
        return std::sqrt(s/(float)m.size())+1e-6f;
    }
    static float z(float v, float mn, float sd) { return std::clamp((v-mn)/sd,-3.0f,3.0f); }
    static float to5(float zv) { return std::clamp((zv+3.0f)/6.0f*5.0f,0.0f,5.0f); }
    std::vector<std::string> all_keys() const {
        std::vector<std::string> ks;
        auto add=[&](const auto& mp){ for (auto& kv:mp) if (std::find(ks.begin(),ks.end(),kv.first)==ks.end()) ks.push_back(kv.first); };
        add(eq_vol_); add(cds_); add(geo_acc_); add(news_vel_);
        return ks;
    }
    void recompute_locked() {
        float emn=cmean(eq_vol_),esd=cstd(eq_vol_,emn);
        float cmn=cmean(cds_),csd=cstd(cds_,cmn);
        int mx_news=1; for (auto& kv:news_vel_) mx_news=std::max(mx_news,kv.second);
        std::unordered_map<std::string,GMSIScore> ns;
        for (const auto& iso2 : all_keys()) {
            GMSIScore sc;
            float ev = eq_vol_.count(iso2)?eq_vol_.at(iso2):emn;
            float cd = cds_.count(iso2)?cds_.at(iso2):cmn;
            sc.eq_vol_z = z(ev,emn,esd); sc.cds_z = z(cd,cmn,csd);
            if (geo_acc_.count(iso2)&&geo_cnt_.count(iso2)) sc.geo_risk = std::clamp(geo_acc_.at(iso2)/geo_cnt_.at(iso2)*0.5f,0.0f,2.5f);
            if (news_vel_.count(iso2)) sc.news_vel = (float)news_vel_.at(iso2)/(float)mx_news;
            sc.score = std::clamp(
                w_.equity_vol*to5(sc.eq_vol_z) + w_.cds_spread*to5(sc.cds_z) +
                w_.geo_risk*(sc.geo_risk/2.5f*5.0f) + w_.news_velocity*(sc.news_vel*5.0f), 0.0f, 5.0f);
            ns[iso2]=sc;
        }
        std::scoped_lock l{read_m_}; scores_=std::move(ns);
    }
    void seed_baseline() {
        struct S { const char* iso; float ev; float cds; float geo; };
        static constexpr S SEEDS[] = {
            {"US",0.15f,22.f,0.5f},{"GB",0.14f,30.f,0.6f},{"DE",0.16f,29.f,0.7f},{"JP",0.13f,26.f,0.4f},
            {"CN",0.20f,75.f,1.5f},{"FR",0.16f,45.f,0.8f},{"IN",0.18f,85.f,1.2f},{"BR",0.22f,165.f,1.4f},
            {"RU",0.35f,580.f,4.5f},{"TR",0.28f,320.f,2.8f},{"AR",0.42f,850.f,3.0f},{"UA",0.55f,1200.f,5.0f},
        };
        for (const auto& s : SEEDS) { eq_vol_[s.iso]=s.ev; cds_[s.iso]=s.cds; geo_acc_[s.iso]=s.geo; geo_cnt_[s.iso]=1; }
        dirty_=true; maybe_recompute();
    }
};

// ═══════════════════════════════ SECTION 1: GLOBE LAYER ════════════════════════
struct GlobeMarketStat { std::string label, value; };
struct GlobeAlert { std::string text; int severity{0}; };
struct GlobeOverlay { const char* label; bool active{false}; };

class GlobeLayer {
public:
    struct Config {
        int width{1280}, height{640};
        float idle_rads{0.03f};
        double idle_resume_s{20.0};
        float left_ratio{0.18f}, right_ratio{0.20f};
    };

    explicit GlobeLayer(AppStateBus& bus, Config cfg = {}) : bus_(bus), cfg_(cfg) {
        init_fbo(); seed_stats();
        MACRO_LOG("[Globe] STUB renderer active (osgEarth not linked in this build)");
    }
    ~GlobeLayer() { cleanup_fbo(); }

    void render(float w, float h) {
        update_rotation();
        float lw = w*cfg_.left_ratio, rw = w*cfg_.right_ratio, cw = w-lw-rw;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
        ImGui::BeginChild("##GlobeRow", {w,h}, false, ImGuiWindowFlags_NoScrollbar);
        render_left(lw,h); ImGui::SameLine(0,0);
        render_centre(cw,h); ImGui::SameLine(0,0);
        render_right(rw,h);
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    void drill_to_country(const std::string& iso2, const std::string& name, const std::string& iso3, std::array<double,4> bbox) {
        ctx_.resolution=GeoResolution::Country; ctx_.country_iso2=iso2; ctx_.country_iso3=iso3;
        ctx_.country_name=name; ctx_.bbox=bbox;
        rotating_=false; last_interact_=std::chrono::steady_clock::now();
        bus_.publish(ctx_);
        push_alert("Selected: "+name+" ("+iso2+")",0);
    }
    void drill_up() {
        switch (ctx_.resolution) {
            case GeoResolution::Ground: case GeoResolution::City: ctx_.resolution=GeoResolution::State; ctx_.city_name.reset(); break;
            case GeoResolution::State: ctx_.resolution=GeoResolution::Country; ctx_.admin1_name.reset(); break;
            case GeoResolution::Country: ctx_.resolution=GeoResolution::Continent; ctx_.country_name.clear(); break;
            case GeoResolution::Continent: ctx_.resolution=GeoResolution::World; ctx_.continent.clear(); break;
            case GeoResolution::World: break;
        }
        bus_.publish(ctx_);
    }
    void push_alert(const std::string& txt, int sev) {
        std::scoped_lock l{alert_m_};
        alerts_.push_front({txt,sev});
        if (alerts_.size()>12) alerts_.pop_back();
    }
    void ingest_record(const NormalizedRecord& rec) {
        gmsi_.ingest_record(rec);
        if (rec.severity>=3) push_alert(rec.headline.substr(0,std::min(rec.headline.size(),std::size_t{72}))+"...", rec.severity);
    }
    const GeoSelectionContext& context() const noexcept { return ctx_; }

private:
    AppStateBus& bus_;
    Config cfg_;
    GeoSelectionContext ctx_;
    GMSIComputer gmsi_;
    GLuint fbo_{0}, color_tex_{0}, depth_rb_{0};
    int fbo_w_{0}, fbo_h_{0};
    bool rotating_{true};
    double angle_{0.0};
    std::chrono::steady_clock::time_point last_interact_{}, last_frame_{std::chrono::steady_clock::now()};
    GlobeOverlay overlays_[6] = {
        {"Capital Flows",true},{"Sovereign Yields",false},{"Volatility Skew",true},
        {"Liquidity Nodes",false},{"CPI Differentials",false},{"FX Swap Lines",false}
    };
    std::vector<GlobeMarketStat> stats_;
    std::deque<GlobeAlert> alerts_;
    std::mutex alert_m_;

    void render_left(float w, float h) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_SECONDARY);
        ImGui::BeginChild("##GL", {w,h}, true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(Theme::TEXT_MUTED, "SECTION 1 - MACRO GLOBE");
        ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "OVERLAY LAYERS");
        for (auto& ov : overlays_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ov.active?Theme::ACCENT_BLUE_SOLID:Theme::BG_ELEVATED);
            std::string lbl = std::string(ov.active?"[X] ":"[ ] ") + ov.label;
            if (ImGui::Button(lbl.c_str(), {w-18.0f,20.0f})) ov.active=!ov.active;
            ImGui::PopStyleColor(); ImGui::Spacing();
        }
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "BREADCRUMB");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+w-18.0f);
        ImGui::TextColored(Theme::TEXT_SECONDARY, "%s", ctx_.breadcrumb().c_str());
        ImGui::PopTextWrapPos();
        if (ctx_.resolution!=GeoResolution::World) { ImGui::Spacing(); if (ImGui::Button("< Back",{w-18.0f,20.0f})) drill_up(); }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_centre(float w, float h) {
        if ((int)w!=fbo_w_ || (int)(h-48)!=fbo_h_) resize_fbo((int)w,(int)(h-48));
        render_to_fbo();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_PRIMARY);
        ImGui::BeginChild("##GC", {w,h}, false, ImGuiWindowFlags_NoScrollbar);
        float gh = h-48.0f;
        render_stub(w, gh);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        std::string bc = ctx_.breadcrumb();
        ImVec2 tp = {wp.x+10.0f, wp.y+8.0f};
        dl->AddRectFilled({tp.x-4,tp.y-2},{tp.x+ImGui::CalcTextSize(bc.c_str()).x+8,tp.y+16}, IM_COL32(10,14,20,180), 3.0f);
        dl->AddText(tp, ImGui::ColorConvertFloat4ToU32(Theme::TEXT_SECONDARY), bc.c_str());
        ImGui::SetCursorPosY(gh+4.0f); ImGui::SetCursorPosX(8.0f);
        for (auto& ov : overlays_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ov.active?Theme::ACCENT_BLUE_SOLID:Theme::BG_ELEVATED);
            std::string chip = std::string(ov.active?"[X] ":"[ ] ") + ov.label;
            if (ImGui::SmallButton(chip.c_str())) ov.active=!ov.active;
            ImGui::PopStyleColor(); ImGui::SameLine(0,6);
        }
        if (ImGui::IsWindowHovered() && (ImGui::GetIO().MouseWheel!=0.f || ImGui::IsMouseDragging(0))) {
            rotating_=false; last_interact_=std::chrono::steady_clock::now();
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_right(float w, float h) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_SECONDARY);
        ImGui::BeginChild("##GR", {w,h}, true, ImGuiWindowFlags_NoScrollbar);
        std::string loc = "LOCATION CONTEXT";
        loc += (ctx_.resolution!=GeoResolution::World && !ctx_.country_name.empty()) ? (": "+ctx_.country_name) : ": NY / LND";
        ImGui::TextColored(Theme::TEXT_MUTED, "%s", loc.c_str());
        ImGui::Separator(); ImGui::Spacing();
        for (auto& s : stats_) {
            ImGui::TextColored(Theme::TEXT_SECONDARY, "%-18s", s.label.c_str());
            ImGui::SameLine(w*0.52f);
            ImGui::TextColored(Theme::TEXT_PRIMARY, "%s", s.value.c_str());
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "ALERT STREAM"); ImGui::Spacing();
        ImGui::BeginChild("##Alts", {w-16.0f, h-ImGui::GetCursorPosY()-8.0f}, false, ImGuiWindowFlags_NoScrollbar);
        { std::scoped_lock l{alert_m_}; for (auto& a : alerts_) ImGui::TextColored(Theme::severity_color(a.severity), "> %s", a.text.c_str()); }
        ImGui::EndChild();
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_stub(float w, float h) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float yo = ImGui::GetCursorPosY();
        float cx=wp.x+w*0.5f, cy=wp.y+yo+h*0.5f, r=std::min(w,h)*0.42f;
        dl->AddRectFilled({wp.x,wp.y+yo},{wp.x+w,wp.y+yo+h}, IM_COL32(4,8,16,255));
        dl->AddCircleFilled({cx,cy}, r, IM_COL32(10,22,45,255), 128);
        for (int lat=-60; lat<=60; lat+=30) {
            float yr=r*std::sin(lat*3.14159f/180.f);
            float xr=std::sqrt(std::max(0.f,r*r-yr*yr));
            dl->AddLine({cx-xr,cy-yr},{cx+xr,cy-yr}, lat==0?IM_COL32(45,212,255,40):IM_COL32(45,212,255,18), lat==0?0.8f:0.4f);
        }
        for (int i=0;i<12;++i) {
            double ang=angle_+i*(3.14159265/6.0);
            float cosv=(float)std::cos(ang), sinv=(float)std::sin(ang); (void)sinv;
            const int SEG=48; ImVec2 pts[SEG+1];
            for (int s=0;s<=SEG;++s) { float t=(float)s/SEG, th=t*2.f*3.14159f; pts[s]={cx+r*cosv*std::cos(th), cy+r*std::sin(th)}; }
            dl->AddPolyline(pts, SEG+1, IM_COL32(45,212,255,15), 0, 0.4f);
        }
        struct HP { const char* iso; float sx,sy; int sev; };
        static constexpr HP HS[] = {
            {"UA",0.565f,0.38f,5},{"RU",0.580f,0.30f,4},{"IR",0.596f,0.46f,4},
            {"AR",0.378f,0.65f,3},{"TR",0.578f,0.41f,3},{"CN",0.693f,0.40f,2},{"US",0.262f,0.38f,1}
        };
        float pulse = std::fmod(std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count(), 2.0f)/2.0f;
        for (const auto& hs : HS) {
            float hx=wp.x+hs.sx*w, hy=wp.y+yo+hs.sy*h;
            ImVec4 c = Theme::severity_color(hs.sev);
            ImU32 cu = ImGui::ColorConvertFloat4ToU32(c);
            if (hs.sev>=3) dl->AddCircle({hx,hy}, 4.f+pulse*14.f, ImGui::ColorConvertFloat4ToU32({c.x,c.y,c.z,(1.f-pulse)*0.6f}), 32, 1.2f);
            dl->AddCircleFilled({hx,hy}, 3.5f, cu, 16);
        }
        dl->AddCircle({cx,cy}, r, IM_COL32(45,212,255,55), 128, 1.0f);
        ImGui::Dummy({w,h});
    }
    void update_rotation() {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now-last_frame_).count();
        last_frame_=now;
        if (!rotating_) { double idle=std::chrono::duration<double>(now-last_interact_).count(); if (idle>cfg_.idle_resume_s) rotating_=true; }
        if (rotating_) angle_ += cfg_.idle_rads*dt;
    }
    void init_fbo() { fbo_w_=cfg_.width; fbo_h_=cfg_.height; create_fbo(fbo_w_,fbo_h_); }
    void create_fbo(int w, int h) {
        glGenFramebuffers(1,&fbo_); glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
        glGenTextures(1,&color_tex_); glBindTexture(GL_TEXTURE_2D,color_tex_);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color_tex_,0);
        glGenRenderbuffers(1,&depth_rb_); glBindRenderbuffer(GL_RENDERBUFFER,depth_rb_);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth_rb_);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    void resize_fbo(int w, int h) { cleanup_fbo(); fbo_w_=w; fbo_h_=h; create_fbo(w,h); }
    void cleanup_fbo() {
        if (fbo_) glDeleteFramebuffers(1,&fbo_);
        if (color_tex_) glDeleteTextures(1,&color_tex_);
        if (depth_rb_) glDeleteRenderbuffers(1,&depth_rb_);
        fbo_=color_tex_=depth_rb_=0;
    }
    void render_to_fbo() {
        if (!fbo_) return;
        glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
        glViewport(0,0,fbo_w_,fbo_h_);
        glClearColor(0.039f,0.055f,0.078f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    void seed_stats() {
        stats_ = {
            {"VIX / VIY","13.3"},{"MOVE Index","94.8"},{"SOFR 3M","5.11%"},
            {"US 10Y Real","2.18%"},{"USD/JPY","152.4"},{"Net Liquidity","$5.4T"},{"GMSI","\xe2\x80\x94"}
        };
        alerts_.push_back({"NY Open Volatility Snap Detected",2});
        alerts_.push_back({"Vanna flows accelerating in ITS",1});
        alerts_.push_back({"Rates volatility stabilising",0});
    }
};

// ═══════════════════════════════ SECTION 2: FEED MODULE ═══════════════════════
class FeedModule {
public:
    static constexpr std::size_t MAX_ARTICLES = 50;
    explicit FeedModule(FeedDomain d) : domain_(d) {}

    void replace_articles(std::vector<ArticleRecord> arts) {
        std::sort(arts.begin(), arts.end(), [](const ArticleRecord& a, const ArticleRecord& b){ return a.published_at > b.published_at; });
        if (arts.size() > MAX_ARTICLES) arts.resize(MAX_ARTICLES);
        articles_ = std::deque<ArticleRecord>(arts.begin(), arts.end());
        is_loading_ = false; last_refresh_ = std::chrono::system_clock::now(); error_msg_.clear();
    }
    void prepend_article(ArticleRecord art) {
        articles_.push_front(std::move(art));
        if (articles_.size()>MAX_ARTICLES) articles_.pop_back();
        last_refresh_ = std::chrono::system_clock::now();
    }
    void set_loading(bool v, std::string tier="") { is_loading_=v; if (!tier.empty()) tier_label_=std::move(tier); }
    void set_error(const std::string& m) { is_loading_=false; error_msg_=m; }
    void clear_error() { error_msg_.clear(); }
    [[nodiscard]] int article_count() const noexcept { return (int)articles_.size(); }

    void render(float width) {
        render_header(width);
        if (collapsed_) return;
        if (is_loading_) { render_loading(width); return; }
        if (!error_msg_.empty()) { render_error(); return; }
        if (articles_.empty()) { render_empty(); return; }
        for (int i=0;i<(int)articles_.size();++i) render_card(articles_[(std::size_t)i], width, i);
        auto age = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-last_refresh_).count();
        ImGui::TextColored(age>300?Theme::TEXT_STALE:Theme::TEXT_MUTED, "  %d articles \xc2\xb7 refreshed %llds ago \xc2\xb7 %s",
            article_count(), (long long)age, tier_label_.c_str());
        ImGui::Dummy({0,6.0f});
    }

private:
    FeedDomain domain_;
    std::deque<ArticleRecord> articles_;
    bool is_loading_{false}, collapsed_{false};
    std::string tier_label_{"GLOBAL"}, error_msg_;
    std::chrono::system_clock::time_point last_refresh_{std::chrono::system_clock::now()};
    int hovered_idx_{-1};

    void render_header(float width) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float hh = 22.0f;
        ImVec4 acc = accent();
        dl->AddRectFilled({p.x,p.y},{p.x+width,p.y+hh}, IM_COL32(13,17,23,255));
        dl->AddRectFilled({p.x,p.y},{p.x+3.0f,p.y+hh}, ImGui::ColorConvertFloat4ToU32(acc));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8.0f); ImGui::SetCursorPosY(ImGui::GetCursorPosY()+3.0f);
        ImGui::TextColored(acc, "%s", feed_domain_label(domain_));
        float rx = width - 170.0f;
        ImGui::SameLine(rx);
        if (!tier_label_.empty()) ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "[%s]", tier_label_.c_str());
        ImGui::SameLine(0,10);
        if (is_loading_) {
            static int tk=0; static const char* SP[]={"\xe2\x97\x90","\xe2\x97\x93","\xe2\x97\x91","\xe2\x97\x92"};
            ImGui::TextColored(Theme::SEV_ELEVATED, "%s", SP[(tk++/6)%4]);
        } else ImGui::TextColored(Theme::TEXT_MUTED, "%d", article_count());
        ImGui::SameLine(0,10);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::BG_ELEVATED);
        if (ImGui::SmallButton(collapsed_?"[+]":"[-]")) collapsed_=!collapsed_;
        ImGui::PopStyleColor(2);
        ImGui::Dummy({0,0});
        p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine({p.x,p.y},{p.x+width,p.y}, IM_COL32(42,50,64,200), 1.0f);
    }
    void render_card(const ArticleRecord& art, float width, int idx) {
        bool hovered = (hovered_idx_==idx);
        float ch = card_h(art, hovered);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled({p.x,p.y},{p.x+width,p.y+ch}, hovered?ImGui::ColorConvertFloat4ToU32(Theme::BG_ELEVATED):IM_COL32(10,14,20,220));
        ImVec4 sc = Theme::severity_color(art.severity);
        dl->AddRectFilled({p.x,p.y},{p.x+3.0f,p.y+ch}, ImGui::ColorConvertFloat4ToU32(sc));
        float indent = 10.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+indent); ImGui::SetCursorPosY(ImGui::GetCursorPosY()+5.0f);
        ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "%-13.13s", art.source_name.c_str());
        ImGui::SameLine(0,8); ImGui::TextColored(Theme::TEXT_MUTED, "%s", fmt_ts(art.published_at).c_str());
        ImGui::SameLine(0,8); ImGui::TextColored(Theme::TEXT_MUTED, "\xc2\xb7"); ImGui::SameLine(0,8);
        ImGui::TextColored(Theme::TEXT_MUTED, "%s", art.geo_label.c_str());
        for (const auto& tag : art.metric_tags) { ImGui::SameLine(0,10); render_pill(tag); }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+indent);
        float wrap_w = width - indent*2.0f;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+wrap_w);
        ImGui::TextColored(Theme::TEXT_PRIMARY, "%s", art.headline.c_str());
        ImGui::PopTextWrapPos();
        if (!art.snippet.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+indent);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+wrap_w);
            std::string snip = art.snippet;
            if (!hovered && snip.size()>260) snip = snip.substr(0,260)+"...";
            ImGui::TextColored(Theme::TEXT_SECONDARY, "%s", snip.c_str());
            ImGui::PopTextWrapPos();
        }
        if (hovered && !art.source_url.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+indent);
            std::string su = art.source_url.substr(0, std::min(art.source_url.size(), std::size_t{80}));
            ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "-> %s", su.c_str());
        }
        ImGui::Dummy({0,5.0f});
        p = ImGui::GetCursorScreenPos();
        dl->AddLine({p.x,p.y-1.0f},{p.x+width,p.y-1.0f}, IM_COL32(42,50,64,100), 1.0f);
        ImVec2 card_min={p.x,p.y-ch-5.0f}, card_max={p.x+width,p.y};
        if (ImGui::IsMouseHoveringRect(card_min,card_max)) hovered_idx_=idx;
        else if (hovered_idx_==idx) hovered_idx_=-1;
    }
    void render_pill(const MetricTag& tag) {
        std::string txt = tag.label + " " + tag.value;
        ImVec2 sz = ImGui::CalcTextSize(txt.c_str());
        ImVec4 c = Theme::severity_color(tag.severity);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float pw = sz.x+10.0f, ph = sz.y+4.0f;
        dl->AddRectFilled({p.x-2,p.y-1},{p.x+pw,p.y+ph-1}, ImGui::ColorConvertFloat4ToU32({c.x,c.y,c.z,0.18f}), Theme::ROUNDING_CHIP);
        dl->AddRect({p.x-2,p.y-1},{p.x+pw,p.y+ph-1}, ImGui::ColorConvertFloat4ToU32({c.x,c.y,c.z,0.40f}), Theme::ROUNDING_CHIP, 0, 0.6f);
        ImGui::TextColored(c, "%s", txt.c_str());
        ImGui::Dummy({pw-sz.x+2.0f,0});
    }
    void render_loading(float width) {
        ImGui::Dummy({0,8.0f}); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+14.0f);
        ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "  Fetching %s data for this viewport...", tier_label_.c_str());
        ImGui::Dummy({width,24.0f});
    }
    void render_error() {
        ImGui::Dummy({0,4.0f}); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+12.0f);
        ImGui::TextColored(Theme::SEV_CRITICAL, "  x  %s", error_msg_.c_str());
        ImGui::Dummy({0,8.0f});
    }
    void render_empty() {
        ImGui::Dummy({0,4.0f}); ImGui::SetCursorPosX(ImGui::GetCursorPosX()+12.0f);
        ImGui::TextColored(Theme::TEXT_MUTED, "  No %s data for current viewport.", tier_label_.c_str());
        ImGui::Dummy({0,8.0f});
    }
    static float card_h(const ArticleRecord& art, bool hovered) noexcept {
        float h=52.0f;
        if (!art.snippet.empty()) h += hovered?80.0f:38.0f;
        if (hovered && !art.source_url.empty()) h += 20.0f;
        return h;
    }
    static std::string fmt_ts(const std::chrono::system_clock::time_point& tp) {
        auto tt = std::chrono::system_clock::to_time_t(tp);
        std::tm gm{}; gmtime_r(&tt,&gm);
        char buf[10]; std::strftime(buf,sizeof(buf),"%H:%M UTC",&gm);
        return buf;
    }
    [[nodiscard]] ImVec4 accent() const noexcept {
        switch (domain_) {
            case FeedDomain::MacroDevelopments: return Theme::SEV_ELEVATED;
            case FeedDomain::MicroDevelopments: return Theme::DIR_BULLISH;
            case FeedDomain::GeopoliticalTensions: return Theme::SEV_HIGH;
            case FeedDomain::CentralBankUpdates: return Theme::ACCENT_CYAN_DIM;
            case FeedDomain::MonetaryPolicy: return Theme::SEV_ELEVATED;
            case FeedDomain::GlobalRegionalNews: return Theme::TEXT_SECONDARY;
            case FeedDomain::MilitaryWarNews: return Theme::SEV_CRITICAL;
        }
        return Theme::TEXT_MUTED;
    }
};

// ═══════════════════════════════ SECTION 2: TABLES LAYER ═══════════════════════
class TablesLayer {
public:
    static constexpr int DEBOUNCE_MS = 2000;

    TablesLayer(AppStateBus& bus, const Secrets& sec) : bus_(bus), fetcher_(std::make_unique<GeoScopedFetcher>(sec)) {
        for (int d=0; d<FEED_DOMAIN_COUNT; ++d) mods_[d] = std::make_unique<FeedModule>((FeedDomain)d);
        bus_tok_ = bus_.subscribe([this](const GeoSelectionContext& ctx){ on_context(ctx); });
        fetcher_->start();
    }
    ~TablesLayer() { bus_.unsubscribe(bus_tok_); fetcher_->stop(); }

    void tick() {
        if (pending_.has_value()) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-change_at_).count();
            if (ms >= DEBOUNCE_MS) {
                fetcher_->request_fetch(*pending_);
                current_ = *pending_; pending_.reset();
                fetching_ = true;
                tier_label_ = tier_str(fetch_tier(current_.resolution));
            }
        }
        for (auto& r : fetcher_->drain_results()) {
            int di = (int)r.domain; if (di<0||di>=FEED_DOMAIN_COUNT) continue;
            auto& m = *mods_[di];
            if (r.is_loading) m.set_loading(true, r.fetch_tier_label);
            else if (r.is_error) m.set_error(r.error_msg);
            else { m.replace_articles(std::move(r.articles)); m.set_loading(false); }
        }
        if (fetching_ && !fetcher_->is_fetching()) fetching_=false;
    }

    void ingest(const NormalizedRecord& rec) {
        ArticleRecord art = to_article(rec);
        if (art.headline.empty()) return;
        mods_[(int)route(rec)]->prepend_article(std::move(art));
    }

    void render(float x, float y, float width, float height) {
        ImGui::SetNextWindowPos({x,y});
        ImGui::SetNextWindowSize({width,height});
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings|
            ImGuiWindowFlags_NoBringToDisplayFrontOnFocus;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_PRIMARY);
        if (ImGui::Begin("##S2Feed", nullptr, F)) {
            render_header(width-16.0f);
            render_bar(width-16.0f);
            const float scroll_h = height - 46.0f - 4.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_PRIMARY);
            ImGui::BeginChild("##FeedScroll", {width-8.0f, scroll_h});
            float fw = ImGui::GetContentRegionAvail().x;
            for (int d=0;d<FEED_DOMAIN_COUNT;++d) mods_[d]->render(fw);
            ImGui::EndChild(); ImGui::PopStyleColor();
        }
        ImGui::End(); ImGui::PopStyleColor();
    }

private:
    AppStateBus& bus_;
    AppStateBus::Token bus_tok_{};
    std::unique_ptr<GeoScopedFetcher> fetcher_;
    std::unique_ptr<FeedModule> mods_[FEED_DOMAIN_COUNT];
    std::optional<GeoSelectionContext> pending_;
    std::chrono::steady_clock::time_point change_at_{};
    GeoSelectionContext current_;
    bool fetching_{false};
    std::string tier_label_{"GLOBAL"};

    void on_context(const GeoSelectionContext& ctx) {
        if (ctx.resolution==GeoResolution::Ground) return;
        pending_ = ctx; change_at_ = std::chrono::steady_clock::now(); fetching_ = true;
        for (int d=0;d<FEED_DOMAIN_COUNT;++d) mods_[d]->set_loading(true, tier_str(fetch_tier(ctx.resolution)));
    }
    void render_header(float width) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_SECONDARY);
        ImGui::BeginChild("##S2Hdr", {width,22.0f}, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPosY(3.0f); ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "SECTION 2 - MACROECONOMIC INTELLIGENCE & NEWS STREAM");
        ImGui::SameLine(width-290.0f);
        ImGui::TextColored(Theme::TEXT_MUTED, "Scope:"); ImGui::SameLine(0,4);
        ImGui::TextColored(Theme::ACCENT_CYAN, "%s", current_.selected_name().c_str());
        ImGui::SameLine(0,8); ImGui::TextColored(Theme::TEXT_MUTED, "Tier:"); ImGui::SameLine(0,4);
        ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "[%s]", tier_label_.c_str());
        ImGui::SameLine(0,14);
        if (!fetching_ && !pending_.has_value()) ImGui::TextColored(Theme::DIR_BULLISH, "\xe2\x97\x8f LIVE");
        else if (pending_.has_value()) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-change_at_).count();
            int rem = (int)std::max(0LL, (long long)DEBOUNCE_MS - ms);
            ImGui::TextColored(Theme::SEV_ELEVATED, "\xe2\x97\x89 REFRESHING IN %dms", rem);
        } else ImGui::TextColored(Theme::SEV_ELEVATED, "\xe2\x97\x89 FETCHING...");
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_bar(float width) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float bh=2.0f;
        dl->AddRectFilled({p.x,p.y},{p.x+width,p.y+bh}, IM_COL32(42,50,64,180));
        if (fetching_) {
            if (pending_.has_value()) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-change_at_).count();
                float prog = std::clamp((float)ms/DEBOUNCE_MS, 0.0f, 1.0f);
                dl->AddRectFilled({p.x,p.y},{p.x+width*prog,p.y+bh}, ImGui::ColorConvertFloat4ToU32(Theme::SEV_ELEVATED));
            } else {
                float t = std::fmod(std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count(), 1.6f)/1.6f;
                float s2 = std::max(0.0f,t-0.30f), e2 = std::min(1.0f,t+0.30f);
                dl->AddRectFilled({p.x+width*s2,p.y},{p.x+width*e2,p.y+bh}, ImGui::ColorConvertFloat4ToU32(Theme::ACCENT_CYAN));
            }
        }
        ImGui::Dummy({width,bh+2.0f});
    }
    static FeedDomain route(const NormalizedRecord& rec) {
        const auto& d = rec.domain;
        if (d=="econ_calendar") return FeedDomain::MacroDevelopments;
        if (d=="monetary_policy") return FeedDomain::MonetaryPolicy;
        if (d=="central_bank") return FeedDomain::CentralBankUpdates;
        if (d=="geopolitics") return FeedDomain::GeopoliticalTensions;
        if (d=="sector_data"||d=="positioning") return FeedDomain::MicroDevelopments;
        const auto& h = rec.headline;
        auto ci = [&](const char* kw){ return h.find(kw)!=std::string::npos; };
        if (ci("militar")||ci("war")||ci("conflict")||ci("troop")||ci("attack")||rec.severity>=4) return FeedDomain::MilitaryWarNews;
        if (ci("central bank")||ci("ECB")||ci("Fed ")||ci("BOE")||ci("BOJ")) return FeedDomain::CentralBankUpdates;
        if (ci("rate")||ci("bps")||ci("hike")||ci("cut")) return FeedDomain::MonetaryPolicy;
        if (ci("GDP")||ci("inflation")||ci("CPI")||ci("employment")||ci("NFP")) return FeedDomain::MacroDevelopments;
        if (ci("earnings")||ci("revenue")||ci("sector")||ci("company")) return FeedDomain::MicroDevelopments;
        return FeedDomain::GlobalRegionalNews;
    }
    static ArticleRecord to_article(const NormalizedRecord& rec) {
        ArticleRecord a;
        a.domain=route(rec); a.source_name=rec.source_name; a.headline=rec.headline;
        try {
            auto j = nlohmann::json::parse(rec.payload_json);
            for (const char* k : {"description","summary","notes","text"})
                if (j.contains(k) && j[k].is_string()) { a.snippet = j[k].get<std::string>().substr(0,300); break; }
        } catch (...) {}
        a.id=rec.record_id; a.geo_label=rec.geo.country_iso2.value_or("Global");
        a.fetch_tier_label="PIPELINE"; a.is_geo_fetched=false; a.severity=rec.severity; a.published_at=rec.timestamp;
        return a;
    }
    static std::string tier_str(int t) noexcept {
        switch (t) { case 0: return "GLOBAL"; case 1: return "CONTINENT"; case 2: return "COUNTRY"; case 3: return "LOCAL"; default: return "SATELLITE"; }
    }
};

// ═══════════════════════════════ SECTION 3: FACTOR MODEL ═══════════════════════
enum class MacroFactor : int { RealRates=0, InflationSurp=1, GrowthProxy=2, USDIndex=3, CreditSpread=4, OilPrice=5, COUNT=6 };
static constexpr int N_FACTORS = (int)MacroFactor::COUNT;
static constexpr std::array<std::string_view,11> GICS_SECTORS = {
    "Energy","Materials","Industrials","Cons.Disc","Cons.Staples","Healthcare","Financials","InfoTech","CommSvcs","Utilities","RealEstate"
};
static constexpr int N_SECTORS = 11;

struct RegimeResult {
    std::string_view sector, region;
    float regime_score{0}, conviction{0}, r_squared{0}, breadth{0};
    std::array<float,N_FACTORS> factor_betas{}, factor_contributions{};
};

class TerrainGrid {
public:
    TerrainGrid(std::size_t rows, std::size_t cols) : storage_(rows*cols,0.0f), span_(storage_.data(),rows,cols) {}
    float& at(std::size_t r, std::size_t c) noexcept { return span_.at(r,c); }
    float at(std::size_t r, std::size_t c) const noexcept { return span_.at(r,c); }
    std::size_t rows() const noexcept { return span_.extent(0); }
    std::size_t cols() const noexcept { return span_.extent(1); }
private:
    std::vector<float> storage_;
    std::mdspan<float, std::dextents<std::size_t>> span_;
};

class FactorModel {
public:
    struct Config { std::array<float,N_FACTORS> factor_weights{0.25f,0.20f,0.20f,0.15f,0.10f,0.10f}; float breadth_weight{0.20f}; };
    explicit FactorModel(Config c = {}) : cfg_(c) {}
    void set_factor(MacroFactor f, float v) { current_factors_[(int)f]=v; }
    void update_returns(const Eigen::MatrixXf& sr, const Eigen::MatrixXf& fr) { sector_returns_=sr; factor_returns_=fr; dirty_=true; }
    bool recompute() {
        if (sector_returns_.rows()<20 || factor_returns_.rows()<20) { MACRO_LOG("[FactorModel] insufficient data - stub scores"); fill_stubs(); return false; }
        results_us_.clear(); results_eu_.clear();
        for (int s=0;s<N_SECTORS;++s) { results_us_.push_back(regress(s,"US")); results_eu_.push_back(regress(s,"Europe")); }
        dirty_=false; return true;
    }
    const std::vector<RegimeResult>& results_us() const { return results_us_; }
    const std::vector<RegimeResult>& results_eu() const { return results_eu_; }
    TerrainGrid build_terrain(const std::vector<RegimeResult>& res, int mesh_cols=32) const {
        TerrainGrid g(N_SECTORS, (std::size_t)mesh_cols);
        for (int s=0;s<N_SECTORS;++s) {
            const auto& r = res[(std::size_t)s];
            float amp=r.regime_score, jagg=1.0f-r.conviction;
            for (int c=0;c<mesh_cols;++c) { float noise=jagg*0.2f*std::sin(c*0.8f+s*1.7f); g.at((std::size_t)s,(std::size_t)c)=amp+noise; }
        }
        return g;
    }
private:
    Config cfg_;
    std::array<float,N_FACTORS> current_factors_{};
    Eigen::MatrixXf sector_returns_, factor_returns_;
    std::vector<RegimeResult> results_us_, results_eu_;
    bool dirty_{true};
    RegimeResult regress(int s, std::string_view region) {
        int T = (int)sector_returns_.rows();
        if (T<20) return stub_score(s,region,0.0f);
        Eigen::VectorXf y = sector_returns_.col(s).head(T);
        Eigen::MatrixXf X(T,N_FACTORS+1); X.col(0)=Eigen::VectorXf::Ones(T);
        for (int f=0;f<N_FACTORS && f<factor_returns_.cols();++f) X.col(f+1)=factor_returns_.col(f).head(T);
        Eigen::VectorXf beta = (X.transpose()*X).ldlt().solve(X.transpose()*y);
        float ym=y.mean(); float ss_t=(y.array()-ym).square().sum(); float ss_r=(y-X*beta).array().square().sum();
        float r2 = (ss_t>1e-8f) ? (1.0f-ss_r/ss_t) : 0.0f;
        RegimeResult res; res.sector=GICS_SECTORS[(std::size_t)s]; res.region=region; res.r_squared=std::clamp(r2,0.0f,1.0f);
        float score=0.0f;
        for (int f=0;f<N_FACTORS;++f) {
            float b=(f+1<(int)beta.size())?beta(f+1):0.0f;
            float c=b*current_factors_[f]*cfg_.factor_weights[f];
            res.factor_betas[f]=b; res.factor_contributions[f]=c; score+=c;
        }
        res.breadth=0.5f;
        score=(1.0f-cfg_.breadth_weight)*score+cfg_.breadth_weight*(res.breadth-0.5f)*2.0f;
        res.conviction=std::clamp(r2*0.8f+0.1f,0.0f,1.0f); res.regime_score=std::clamp(score,-3.0f,3.0f);
        return res;
    }
    void fill_stubs() {
        static constexpr float US[]={0.8f,-0.3f,0.5f,-1.2f,0.4f,0.1f,-0.7f,1.5f,0.6f,-0.2f,-0.9f};
        static constexpr float EU[]={0.3f,0.7f,-0.4f,-0.8f,0.6f,0.2f,0.9f,0.4f,-0.1f,0.5f,0.2f};
        results_us_.clear(); results_eu_.clear();
        for (int s=0;s<N_SECTORS;++s) { results_us_.push_back(stub_score(s,"US",US[s])); results_eu_.push_back(stub_score(s,"Europe",EU[s])); }
    }
    static RegimeResult stub_score(int s, std::string_view r, float sc) {
        RegimeResult x; x.sector=GICS_SECTORS[(std::size_t)s]; x.region=r; x.regime_score=sc; x.conviction=0.4f; x.r_squared=0.3f; x.breadth=0.5f;
        return x;
    }
};

// ═══════════════════════════════ LLM RATIONALE SERVICE ═════════════════════════
struct RegimeRationale {
    std::string sector, region, direction, conviction, rationale;
    std::array<std::string,3> key_drivers{};
    bool from_llm{true};
};

class LLMRationaleService {
public:
    using CB = std::function<void(RegimeRationale)>;
    explicit LLMRationaleService(std::string key) : key_(std::move(key)) {}

    void request(const RegimeResult& r, const std::string& digest, CB cb) {
        std::string ck = cache_key(r);
        { std::scoped_lock lk{c_mtx_}; if (auto it=cache_.find(ck); it!=cache_.end()) { cb(it->second); return; } }
        { std::scoped_lock lk{req_mtx_}; pending_.push({r,digest,std::move(cb),ck}); }
        dirty_.store(true);
    }
    void start() { w_ = std::jthread([this](std::stop_token st){ run(st); }); }
    void stop()  { w_.request_stop(); }
    void dispatch_pending_results() {
        std::queue<std::pair<CB,RegimeRationale>> local;
        { std::scoped_lock lk{res_mtx_}; std::swap(local,results_); }
        while (!local.empty()) { auto& kv=local.front(); kv.first(std::move(kv.second)); local.pop(); }
    }
private:
    struct Req { RegimeResult r; std::string digest; CB cb; std::string ck; };
    std::string key_; HttpClient http_; std::jthread w_;
    std::atomic<bool> dirty_{false};
    mutable std::mutex c_mtx_;
    std::unordered_map<std::string,RegimeRationale> cache_;
    std::mutex req_mtx_;
    std::queue<Req> pending_;
    std::mutex res_mtx_;
    std::queue<std::pair<CB,RegimeRationale>> results_;

    void run(std::stop_token st) {
        MACRO_LOG("[LLMRationale] worker started");
        while (!st.stop_requested()) {
            if (!dirty_.load()) { std::this_thread::sleep_for(std::chrono::milliseconds{80}); continue; }
            Req req;
            {
                std::scoped_lock lk{req_mtx_};
                if (pending_.empty()) { dirty_.store(false); continue; }
                req = std::move(pending_.front()); pending_.pop();
                if (pending_.empty()) dirty_.store(false);
            }
            auto rat = call_api(req.r, req.digest);
            { std::scoped_lock lk{c_mtx_}; cache_[req.ck]=rat; }
            { std::scoped_lock lk{res_mtx_}; results_.push({std::move(req.cb), std::move(rat)}); }
        }
        MACRO_LOG("[LLMRationale] worker stopped");
    }
    RegimeRationale call_api(const RegimeResult& r, const std::string& digest) {
        if (key_.empty()) return fallback(r);
        static const char* FN[] = {"Real Rates","Inflation Surp","Growth Proxy","USD Index","Credit Spread","Oil"};
        std::string fsum;
        for (int i=0;i<N_FACTORS;++i) fsum += std::string(FN[i])+":b="+std::to_string(r.factor_betas[i])+" c="+std::to_string(r.factor_contributions[i])+"; ";
        std::string prompt = std::string("Sector: ")+std::string(r.sector)+" Region: "+std::string(r.region)+
            "\nScore: "+std::to_string(r.regime_score)+" Conv: "+std::to_string(r.conviction)+" R2: "+std::to_string(r.r_squared)+
            "\nFactors: "+fsum+"\nHeadlines: "+digest.substr(0,400)+
            "\n\nReturn ONLY JSON: {\"sector\":str,\"direction\":\"bullish\"|\"bearish\"|\"neutral\","
            "\"conviction\":\"low\"|\"medium\"|\"high\",\"rationale\":str(max 60 words),\"key_drivers\":[str,str,str]}";
        nlohmann::json body = {
            {"model","claude-sonnet-4-6"}, {"max_tokens",300},
            {"system","Return only schema-valid JSON. No markdown. Rationale max 60 words."},
            {"messages",{{{"role","user"},{"content",prompt}}}}
        };
        auto resp = http_.post("https://api.anthropic.com/v1/messages", body.dump(),
            {{"x-api-key",key_},{"anthropic-version","2023-06-01"}});
        if (!resp) { MACRO_LOG("[LLMRationale] API failed: %s", resp.error().message.c_str()); return fallback(r); }
        try {
            auto rj = nlohmann::json::parse(resp->body);
            std::string txt;
            for (auto& blk : rj.value("content", nlohmann::json::array())) if (blk.value("type","")=="text") { txt=blk.value("text",""); break; }
            if (txt.rfind("```",0)==0) {
                auto p=txt.find('\n'); auto e=txt.rfind("```");
                if (p!=std::string::npos && e>p) txt=txt.substr(p+1,e-p-1);
            }
            auto jj = nlohmann::json::parse(txt);
            return parse_rat(jj, r, true);
        } catch (const std::exception& e) { MACRO_LOG("[LLMRationale] parse error: %s", e.what()); return fallback(r); }
    }
    static RegimeRationale parse_rat(const nlohmann::json& j, const RegimeResult& r, bool from_llm) {
        RegimeRationale rat;
        rat.sector=j.value("sector",std::string(r.sector)); rat.region=j.value("region",std::string(r.region));
        rat.direction=j.value("direction","neutral"); rat.conviction=j.value("conviction","low");
        rat.rationale=j.value("rationale",""); rat.from_llm=from_llm;
        int wc=0; std::size_t pos=0;
        for (std::size_t i=0;i<rat.rationale.size();++i)
            if (std::isspace((unsigned char)rat.rationale[i])) { if (++wc>=60) { rat.rationale=rat.rationale.substr(0,pos); break; } pos=i; }
        auto kd = j.value("key_drivers", nlohmann::json::array());
        for (int i=0;i<3&&i<(int)kd.size();++i) rat.key_drivers[i]=kd[i].get<std::string>();
        if (rat.direction!="bullish"&&rat.direction!="bearish") rat.direction="neutral";
        if (rat.conviction!="medium"&&rat.conviction!="high") rat.conviction="low";
        return rat;
    }
    static RegimeRationale fallback(const RegimeResult& r) {
        RegimeRationale rat;
        rat.sector=std::string(r.sector); rat.region=std::string(r.region); rat.from_llm=false;
        rat.direction = r.regime_score>0.3f?"bullish":r.regime_score<-0.3f?"bearish":"neutral";
        rat.conviction = r.conviction>0.65f?"high":r.conviction>0.35f?"medium":"low";
        static const char* FN[]={"real rates","inflation surp","growth","USD","credit spreads","oil"};
        std::array<std::pair<float,int>,N_FACTORS> sv;
        for (int i=0;i<N_FACTORS;++i) sv[i]={std::fabs(r.factor_contributions[i]),i};
        std::sort(sv.begin(),sv.end(),[](auto&a,auto&b){return a.first>b.first;});
        for (int i=0;i<3;++i) rat.key_drivers[i]=FN[sv[i].second];
        rat.rationale = std::string(r.region)+" "+std::string(r.sector)+" regime "+rat.direction+" conv="+rat.conviction+
            " score="+std::to_string(r.regime_score)+". Drivers: "+rat.key_drivers[0]+", "+rat.key_drivers[1]+", "+rat.key_drivers[2]+".";
        return rat;
    }
    static std::string cache_key(const RegimeResult& r) {
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm gm{}; gmtime_r(&tt,&gm);
        char d[12]; std::strftime(d,sizeof(d),"%Y-%m-%d",&gm);
        int sb=(int)(r.regime_score*10), cb=(int)(r.conviction*5);
        return std::string(r.sector)+":"+std::string(r.region)+":"+d+":"+std::to_string(sb)+":"+std::to_string(cb);
    }
};

// ═══════════════════════════════ SECTION 3: TOPOGRAPHY LAYER ═══════════════════
class TopographyLayer {
public:
    enum class Region { US, Europe, Both };
    struct Controls {
        Region region{Region::US}; int breadth_n{150}; bool sector_active[N_SECTORS];
        Controls() { std::fill(sector_active, sector_active+N_SECTORS, true); }
    };

    explicit TopographyLayer(AppStateBus& bus, std::string anthro_key) : bus_(bus), llm_(std::move(anthro_key)) {
        init_gl(); llm_.start();
        bus_tok_ = bus_.subscribe([this](const GeoSelectionContext& c){ on_ctx(c); });
        model_.recompute(); rebuild_mesh(); request_rationales();
        seed_events(); seed_exposures(); seed_drivers();
    }
    ~TopographyLayer() { bus_.unsubscribe(bus_tok_); llm_.stop(); cleanup_gl(); }

    void ingest(const NormalizedRecord& rec) {
        if (rec.domain=="sector_data") {
            try {
                auto j=nlohmann::json::parse(rec.payload_json); float chg=j.value("change_percentage",0.0f);
                static const std::unordered_map<std::string,int> ETF = {
                    {"XLE",0},{"XLB",1},{"XLI",2},{"XLY",3},{"XLP",4},{"XLV",5},{"XLF",6},{"XLK",7},{"XLC",8},{"XLU",9},{"XLRE",10}
                };
                std::string sym=j.value("symbol",""); auto it=ETF.find(sym);
                if (it!=ETF.end()) { needs_recompute_=true; }
            } catch (...) {}
        }
        if (rec.domain=="news"||rec.domain=="econ_calendar") {
            std::scoped_lock l{dig_m_}; digest_+=rec.headline.substr(0,70)+"; ";
            if (digest_.size()>800) digest_=digest_.substr(digest_.size()-800);
        }
    }

    void render(float x, float y, float w, float h) {
        llm_.dispatch_pending_results();
        if (needs_recompute_) { model_.recompute(); rebuild_mesh(); request_rationales(); needs_recompute_=false; }
        ImGui::SetNextWindowPos({x,y}); ImGui::SetNextWindowSize({w,h});
        constexpr ImGuiWindowFlags F = ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BG_PRIMARY);
        if (ImGui::Begin("##Topo", nullptr, F)) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_SECONDARY);
            ImGui::BeginChild("##S3H", {w-16.f,22.f}, false, ImGuiWindowFlags_NoScrollbar);
            ImGui::SetCursorPosX(8.f); ImGui::SetCursorPosY(3.f);
            ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "SECTION 3 - MACRO NEWS SECTOR IMPACT TOPOGRAPHY");
            ImGui::EndChild(); ImGui::PopStyleColor();
            float rem = h - 26.f;
            render_event_bar(w-16.f); rem -= 28.f;
            float th = rem*0.42f; render_terrain(w-16.f,th); rem -= th;
            float sh = rem*0.32f; render_sub_tables(w-16.f,sh); rem -= sh;
            render_regime_table(w-16.f, rem-36.f);
            render_name_search(w-16.f);
        }
        ImGui::End(); ImGui::PopStyleColor();
    }

private:
    AppStateBus& bus_; AppStateBus::Token bus_tok_{};
    GeoSelectionContext geo_{};
    FactorModel model_;
    LLMRationaleService llm_;
    Controls ctrl_;
    std::unordered_map<std::string,RegimeRationale> rats_;
    std::mutex dig_m_; std::string digest_;
    std::atomic<bool> needs_recompute_{false};

    struct MacroEvent { std::string label, desc; bool active{false}; };
    struct SectorExp { std::string gics; float beta{0},impact{0}; };
    struct RiskDriver { std::string driver,level,thresh; bool breached{false}; };
    std::vector<MacroEvent> events_;
    std::vector<SectorExp> exposures_;
    std::vector<RiskDriver> drivers_;
    int active_ev_{0};
    char search_buf_[64]{};

    GLuint t_fbo_{0}, t_tex_{0}, t_depth_{0}, t_vao_{0}, t_vbo_{0}, t_ebo_{0}, t_shader_{0};
    int fbo_w_{1200}, fbo_h_{300};
    struct TV { float x,y,z,sc; };
    std::vector<TV> verts_;
    std::vector<uint32_t> indices_;

    void on_ctx(const GeoSelectionContext& c) {
        if (c.continent=="Europe") ctrl_.region=Region::Europe;
        else if (c.continent=="North America") ctrl_.region=Region::US;
        geo_ = c;
    }
    void render_event_bar(float w) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_ELEVATED);
        ImGui::BeginChild("##EvBar", {w,26.f}, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPosY(4.f); ImGui::SetCursorPosX(8.f);
        for (int i=0;i<(int)events_.size();++i) {
            bool sel=(i==active_ev_);
            ImGui::PushStyleColor(ImGuiCol_Button, sel?Theme::ACCENT_BLUE_SOLID:Theme::BG_SECONDARY);
            if (ImGui::SmallButton(events_[i].label.c_str())) { active_ev_=i; needs_recompute_=true; }
            ImGui::PopStyleColor(); ImGui::SameLine(0,6);
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_terrain(float w, float h) {
        resize_fbo((int)w,(int)(h-40));
        render_to_fbo((int)w,(int)(h-40));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_PRIMARY);
        ImGui::BeginChild("##TV", {w,h}, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(Theme::ACCENT_CYAN_DIM, "[MODEL: MACRO SENSITIVITY 3D SURFACE]");
        ImGui::SameLine(0,16); ImGui::TextColored(Theme::DIR_BEARISH, "RED = Bearish");
        ImGui::SameLine(0,12); ImGui::TextColored(Theme::DIR_BULLISH, "GREEN = Bullish");
        float ih = h - 36.f;
        ImGui::Image((ImTextureID)(uintptr_t)t_tex_, {w,ih}, {0,1}, {1,0});
        ImVec2 imin = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int s=0;s<N_SECTORS;++s) {
            if (!ctrl_.sector_active[s]) continue;
            float xf = (s+0.5f)/(float)N_SECTORS;
            float lx = imin.x+xf*w, ly = imin.y+ih-14.f;
            std::string sn = std::string(GICS_SECTORS[s]).substr(0,7);
            dl->AddText({lx-18.f,ly}, ImGui::ColorConvertFloat4ToU32(Theme::TEXT_MUTED), sn.c_str());
        }
        render_terrain_controls(w);
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_terrain_controls(float w) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_SECONDARY);
        ImGui::BeginChild("##TC", {w,24.f}, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPosY(3.f); ImGui::SetCursorPosX(6.f);
        static const char* RG[]={"US","Europe","Both"};
        for (int i=0;i<3;++i) {
            bool s2=((int)ctrl_.region==i);
            ImGui::PushStyleColor(ImGuiCol_Button, s2?Theme::ACCENT_BLUE_SOLID:Theme::BG_ELEVATED);
            if (ImGui::SmallButton(RG[i])) { ctrl_.region=(Region)i; needs_recompute_=true; }
            ImGui::PopStyleColor(); ImGui::SameLine(0,4);
        }
        ImGui::SameLine(0,12); ImGui::SetNextItemWidth(100.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::BG_ELEVATED);
        if (ImGui::SliderInt("##BN", &ctrl_.breadth_n, 10, 300)) needs_recompute_=true;
        ImGui::PopStyleColor(); ImGui::SameLine(0,4);
        char lbl[24]; std::snprintf(lbl,sizeof(lbl),"Top-%d",ctrl_.breadth_n);
        ImGui::TextColored(Theme::TEXT_MUTED, "%s", lbl);
        ImGui::SameLine(0,12);
        for (int s=0;s<N_SECTORS;++s) {
            bool& on = ctrl_.sector_active[s];
            ImGui::PushStyleColor(ImGuiCol_Button, on?Theme::BG_ELEVATED:Theme::BG_PRIMARY);
            std::string chip = std::string(GICS_SECTORS[s]).substr(0,4);
            if (ImGui::SmallButton(chip.c_str())) { on=!on; needs_recompute_=true; }
            ImGui::PopStyleColor(); ImGui::SameLine(0,3);
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_sub_tables(float w, float h) {
        float half = (w-8.f)*0.5f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_PANEL);
        ImGui::BeginChild("##SE", {half,h}, true, ImGuiWindowFlags_NoScrollbar);
        std::string ev_lbl = active_ev_<(int)events_.size() ? events_[active_ev_].desc : "Macro Event";
        ImGui::TextColored(Theme::TEXT_SECONDARY, "Sector Exposure to %s", ev_lbl.c_str());
        ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "%-22s  %-14s  %s", "GICS SECTOR", "BETA", "IMPACT");
        ImGui::Separator();
        for (auto& se : exposures_) {
            ImGui::TextColored(Theme::TEXT_PRIMARY, "%-22s", se.gics.c_str());
            ImGui::SameLine(160.f); ImGui::TextColored(se.beta<0?Theme::DIR_BEARISH:Theme::DIR_BULLISH, "%+.2fx", se.beta);
            ImGui::SameLine(240.f); ImGui::TextColored(se.impact<0?Theme::DIR_BEARISH:Theme::DIR_BULLISH, "%+.2f%%", se.impact);
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
        ImGui::SameLine(0,8);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_PANEL);
        ImGui::BeginChild("##RD", {half,h}, true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(Theme::TEXT_SECONDARY, "Macro Risk Drivers"); ImGui::Spacing();
        ImGui::TextColored(Theme::TEXT_MUTED, "%-20s  %-14s  %s", "DRIVER", "CURRENT", "THRESHOLD");
        ImGui::Separator();
        for (auto& d : drivers_) {
            ImGui::TextColored(Theme::TEXT_PRIMARY, "%-20s", d.driver.c_str());
            ImGui::SameLine(154.f); ImGui::TextColored(d.breached?Theme::SEV_CRITICAL:Theme::TEXT_PRIMARY, "%-14s", d.level.c_str());
            ImGui::SameLine(254.f); ImGui::TextColored(Theme::TEXT_MUTED, "%s", d.thresh.c_str());
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_regime_table(float w, float h) {
        const auto& res = (ctrl_.region==Region::Europe) ? model_.results_eu() : model_.results_us();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_PANEL);
        ImGui::BeginChild("##RT", {w,h}, true);
        char hdr[128];
        std::snprintf(hdr,sizeof(hdr),"Regime Conviction - %s  (%d names)", ctrl_.region==Region::US?"US":"Europe", ctrl_.breadth_n);
        ImGui::TextColored(Theme::TEXT_SECONDARY, "%s", hdr);
        constexpr ImGuiTableFlags TF = ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("##RTbl", 7, TF, {w-8.f,h-36.f})) {
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::TableSetupColumn("SECTOR", ImGuiTableColumnFlags_WidthFixed, 108);
            ImGui::TableSetupColumn("REGION", ImGuiTableColumnFlags_WidthFixed, 64);
            ImGui::TableSetupColumn("SCORE", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("DIRECTION", ImGuiTableColumnFlags_WidthFixed, 85);
            ImGui::TableSetupColumn("CONVICTION", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("DRIVERS", ImGuiTableColumnFlags_WidthFixed, 200);
            ImGui::TableSetupColumn("RATIONALE", ImGuiTableColumnFlags_WidthStretch, 0);
            ImGui::TableHeadersRow();
            for (int i=0;i<(int)res.size();++i) {
                if (!ctrl_.sector_active[i]) continue;
                const auto& r = res[i];
                std::string key = std::string(r.sector)+":"+std::string(r.region);
                auto it = rats_.find(key);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(Theme::TEXT_PRIMARY, "%s", r.sector.data());
                ImGui::TableNextColumn(); ImGui::TextColored(Theme::TEXT_MUTED, "%s", r.region.data());
                ImGui::TableNextColumn();
                int dir = r.regime_score>0.3f?1:r.regime_score<-0.3f?-1:0;
                ImGui::TextColored(Theme::direction_color(dir), "%+.2f", r.regime_score);
                ImGui::TableNextColumn();
                if (it!=rats_.end()) {
                    int d2 = (it->second.direction=="bullish")?1:(it->second.direction=="bearish")?-1:0;
                    ImGui::TextColored(Theme::direction_color(d2), "%s %s", d2>0?"^":d2<0?"v":"-", it->second.direction.c_str());
                } else ImGui::TextColored(Theme::TEXT_MUTED, "...");
                ImGui::TableNextColumn();
                if (it!=rats_.end()) {
                    const auto& cv = it->second.conviction;
                    ImGui::TextColored(cv=="high"?Theme::CONVICTION_HIGH:cv=="medium"?Theme::CONVICTION_MEDIUM:Theme::CONVICTION_LOW, "%s", cv.c_str());
                }
                ImGui::TableNextColumn();
                if (it!=rats_.end()) ImGui::TextColored(Theme::TEXT_SECONDARY, "%s / %s / %s",
                    it->second.key_drivers[0].c_str(), it->second.key_drivers[1].c_str(), it->second.key_drivers[2].c_str());
                ImGui::TableNextColumn();
                if (it!=rats_.end()) ImGui::TextColored(it->second.from_llm?Theme::TEXT_PRIMARY:Theme::TEXT_SECONDARY,
                    "%s%s", it->second.from_llm?"":"[auto] ", it->second.rationale.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void render_name_search(float w) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BG_SECONDARY);
        ImGui::BeginChild("##NS", {w,32.f}, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPosY(5.f); ImGui::SetCursorPosX(8.f);
        ImGui::TextColored(Theme::TEXT_MUTED, "SINGLE NAME"); ImGui::SameLine(0,12);
        ImGui::SetNextItemWidth(240.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::BG_ELEVATED);
        ImGui::InputTextWithHint("##NSI", "ticker or company name...", search_buf_, sizeof(search_buf_));
        ImGui::PopStyleColor(); ImGui::SameLine(0,8);
        ImGui::SmallButton("SEARCH");
        char wlbl[64]; std::snprintf(wlbl,sizeof(wlbl),"Fuzzy match \xc2\xb7 %d-name universe", ctrl_.breadth_n);
        ImGui::SameLine(0,16); ImGui::TextColored(Theme::TEXT_MUTED, "%s", wlbl);
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    void request_rationales() {
        std::string dig; { std::scoped_lock l{dig_m_}; dig=digest_; }
        auto req = [&](const std::vector<RegimeResult>& res) {
            for (auto& r : res) {
                std::string k = std::string(r.sector)+":"+std::string(r.region);
                llm_.request(r, dig, [this,k](RegimeRationale rat){ rats_[k]=std::move(rat); });
            }
        };
        req(model_.results_us()); req(model_.results_eu());
    }
    void init_gl() { mk_fbo(fbo_w_,fbo_h_); compile_shader(); glGenVertexArrays(1,&t_vao_); glGenBuffers(1,&t_vbo_); glGenBuffers(1,&t_ebo_); MACRO_LOG("[Topography] GL initialized"); }
    void cleanup_gl() {
        if (t_shader_) glDeleteProgram(t_shader_);
        if (t_vao_) glDeleteVertexArrays(1,&t_vao_);
        if (t_vbo_) glDeleteBuffers(1,&t_vbo_);
        if (t_ebo_) glDeleteBuffers(1,&t_ebo_);
        if (t_fbo_) glDeleteFramebuffers(1,&t_fbo_);
        if (t_tex_) glDeleteTextures(1,&t_tex_);
        if (t_depth_) glDeleteRenderbuffers(1,&t_depth_);
    }
    void mk_fbo(int w, int h) {
        glGenFramebuffers(1,&t_fbo_); glBindFramebuffer(GL_FRAMEBUFFER,t_fbo_);
        glGenTextures(1,&t_tex_); glBindTexture(GL_TEXTURE_2D,t_tex_);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t_tex_,0);
        glGenRenderbuffers(1,&t_depth_); glBindRenderbuffer(GL_RENDERBUFFER,t_depth_);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,t_depth_);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    void resize_fbo(int w, int h) {
        if (w==fbo_w_&&h==fbo_h_) return;
        if (t_fbo_) glDeleteFramebuffers(1,&t_fbo_);
        if (t_tex_) glDeleteTextures(1,&t_tex_);
        if (t_depth_) glDeleteRenderbuffers(1,&t_depth_);
        t_fbo_=t_tex_=t_depth_=0; fbo_w_=w; fbo_h_=h; mk_fbo(w,h);
    }
    void compile_shader() {
        const char* vs = "#version 460 core\nlayout(location=0) in vec3 aPos;\nlayout(location=1) in float aScore;\nout float vScore,vHeight;\nuniform mat4 uMVP;\nvoid main(){vScore=aScore;vHeight=aPos.y;gl_Position=uMVP*vec4(aPos,1.0);}\n";
        const char* fs = "#version 460 core\nin float vScore,vHeight;\nout vec4 FragColor;\nvoid main(){\n    float t=clamp((vScore+3.0)/6.0,0.0,1.0);\n    vec3 bear=vec3(0.71,0.35,0.29),neu=vec3(0.70,0.68,0.42),bull=vec3(0.30,0.49,0.47);\n    vec3 col;\n    if(t<0.5) col=mix(bear,neu,t*2.0); else col=mix(neu,bull,(t-0.5)*2.0);\n    float shade=0.55+0.45*clamp((vHeight+0.6)/1.2,0.0,1.0);\n    FragColor=vec4(col*shade,1.0);\n}\n";
        auto mk = [&](GLenum ty, const char* src) -> GLuint {
            GLuint s = glCreateShader(ty);
            glShaderSource(s,1,&src,nullptr); glCompileShader(s);
            GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
            if (!ok) { char log[512]; glGetShaderInfoLog(s,512,nullptr,log); MACRO_LOG("[Topography] shader error: %s", log); }
            return s;
        };
        GLuint v = mk(GL_VERTEX_SHADER,vs), f = mk(GL_FRAGMENT_SHADER,fs);
        t_shader_ = glCreateProgram();
        glAttachShader(t_shader_,v); glAttachShader(t_shader_,f); glLinkProgram(t_shader_);
        glDeleteShader(v); glDeleteShader(f);
    }
    void rebuild_mesh() {
        const auto& res = (ctrl_.region==Region::Europe) ? model_.results_eu() : model_.results_us();
        if (res.empty()) return;
        static constexpr int MC = 48;
        auto grid = model_.build_terrain(res, MC);
        verts_.clear(); indices_.clear();
        int rows = (int)grid.rows(), cols = (int)grid.cols();
        for (int r=0;r<rows;++r) for (int c=0;c<cols;++c) {
            float xf=(float)r/(rows-1)*2.f-1.f, zf=(float)c/(cols-1)*2.f-1.f;
            float h2 = grid.at((std::size_t)r,(std::size_t)c)*0.35f;
            float sc = res[(std::size_t)r].regime_score;
            verts_.push_back({xf,h2,zf,sc});
        }
        for (int r=0;r<rows-1;++r) for (int c=0;c<cols-1;++c) {
            uint32_t tl = (uint32_t)(r*cols+c);
            indices_.insert(indices_.end(), {tl, tl+(uint32_t)cols, tl+1, tl+1, tl+(uint32_t)cols, tl+(uint32_t)cols+1});
        }
        glBindVertexArray(t_vao_);
        glBindBuffer(GL_ARRAY_BUFFER,t_vbo_);
        glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(verts_.size()*sizeof(TV)),verts_.data(),GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,t_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)(indices_.size()*4),indices_.data(),GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(TV),nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,sizeof(TV),(void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    void render_to_fbo(int w, int h) {
        if (verts_.empty()||!t_fbo_) return;
        glBindFramebuffer(GL_FRAMEBUFFER,t_fbo_);
        glViewport(0,0,w,h);
        glClearColor(0.039f,0.055f,0.078f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(t_shader_);
        static const float MVP[16] = {0.85f,0,0,0, 0,0.95f,0,0, -0.35f,0.40f,-0.60f,0, 0,-0.25f,0,1.f};
        glUniformMatrix4fv(glGetUniformLocation(t_shader_,"uMVP"),1,GL_FALSE,MVP);
        glBindVertexArray(t_vao_);
        glDrawElements(GL_TRIANGLES,(GLsizei)indices_.size(),GL_UNSIGNED_INT,nullptr);
        glBindVertexArray(0);
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    void seed_events() {
        events_ = {
            {"NFP +250K [Hot Labor]","NFP +250K Hot Labor Market"},
            {"CPI Above Est","CPI Above Estimate Sticky Inflation"},
            {"Fed Hike +25bps","Federal Reserve Rate Hike +25bps"},
            {"OPEC+ Cut","OPEC+ Production Cut Supply Shock"},
        };
        events_[0].active = true;
    }
    void seed_exposures() {
        exposures_ = {
            {"Information Technology (XLK)",-1.81f,-2.04f}, {"Financials (XLF)",+0.87f,+1.37f},
            {"Energy (XLE)",+0.30f,+0.40f}, {"Real Estate (XLRE)",-1.79f,-1.33f},
        };
    }
    void seed_drivers() {
        drivers_ = {
            {"10Y Yields","4.10%","4.50%",false}, {"USD Index (DXY)","103.6","105.5",false},
            {"WTI Crude","82.8","90.00",false}, {"VIX","13.3","20.0",false},
        };
    }
};

} // namespace macro
