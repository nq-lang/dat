#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// core.hpp — FILE 1 of 4
// Macro Intelligence Terminal — Core Layer
//
// Consolidated from the prior multi-file layout. Contains:
//   • Compat shims for C++23 features absent on GCC-11/12 Codespace images
//     (print/format -> MACRO_LOG/mc::fmt, flat_map -> map, expected shim,
//      generator -> vector-push fallback, mdspan -> .at(r,c) accessor)
//   • AppStateBus         — thread-safe pub/sub for GeoSelectionContext
//   • GeoSelectionContext — globe viewport/zoom state + 2s debounce timestamp
//   • Secrets             — API keys loaded from environment only, never in source
//   • IDataProvider       — base interface + NormalizedRecord schema
//   • HttpClient          — libcurl wrapper returning std::expected
//   • RecordQueue         — MPSC queue draining provider output to UI thread
//
// Providers, UI panels, and the render/App loop live in the other 3 files.
// ═══════════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>
#include <thread>
#include <memory>
#include <algorithm>
#include <cctype>
#include <array>
#include <curl/curl.h>

#define MACRO_LOG(pat, ...)  (void)std::fprintf(stderr, "[macro] " pat "\n", ##__VA_ARGS__)

namespace mc {
inline std::string fmt(const char* pat, ...) {
    va_list a; va_start(a, pat);
    int n = std::vsnprintf(nullptr, 0, pat, a); va_end(a);
    if (n <= 0) return {};
    std::string s(static_cast<std::size_t>(n) + 1, '\0');
    va_start(a, pat); std::vsnprintf(s.data(), s.size(), pat, a); va_end(a);
    s.resize(static_cast<std::size_t>(n));
    return s;
}
} // namespace mc

// ─── std::expected shim (GCC-11/12 lack <expected>) ────────────────────────────
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#  include <expected>
#else
namespace std {
template<class E> struct unexpected {
    E v; explicit unexpected(E e) : v(std::move(e)) {}
};
template<class T, class E>
class expected {
    bool ok_{false};
    union Store { T val; E err; Store(){} ~Store(){} } s_;
public:
    expected()                : ok_(false) { new (&s_.err) E(); }
    expected(T v)              : ok_(true)  { new (&s_.val) T(std::move(v)); }
    expected(unexpected<E> u)  : ok_(false) { new (&s_.err) E(std::move(u.v)); }
    ~expected() { if (ok_) s_.val.~T(); else s_.err.~E(); }
    expected(const expected& o) : ok_(o.ok_) {
        if (ok_) new (&s_.val) T(o.s_.val); else new (&s_.err) E(o.s_.err); }
    expected(expected&& o) : ok_(o.ok_) {
        if (ok_) new (&s_.val) T(std::move(o.s_.val)); else new (&s_.err) E(std::move(o.s_.err)); }
    expected& operator=(expected o) { this->~expected(); new (this) expected(std::move(o)); return *this; }
    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    T&       value()       { if (!ok_) throw std::runtime_error("bad expected"); return s_.val; }
    const T& value() const { if (!ok_) throw std::runtime_error("bad expected"); return s_.val; }
    T& operator*()        { return s_.val; }
    T* operator->()       { return &s_.val; }
    E& error()            { return s_.err; }
    const E& error() const{ return s_.err; }
};
template<class E>
class expected<void,E> {
    bool ok_{true}; std::optional<E> err_;
public:
    expected() : ok_(true) {}
    expected(unexpected<E> u) : ok_(false), err_(std::move(u.v)) {}
    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    E& error() { return *err_; }
    const E& error() const { return *err_; }
};
} // namespace std
#endif
#ifndef MACRO_UNEXPECTED
#define MACRO_UNEXPECTED(e) std::unexpected{e}
#endif

// ─── std::flat_map -> std::map alias ───────────────────────────────────────────
#if defined(__cpp_lib_flat_map) && __cpp_lib_flat_map >= 202207L
#  include <flat_map>
#else
namespace std { template<class K,class V,class C=std::less<K>> using flat_map = std::map<K,V,C>; }
#endif

// ─── std::generator -> vector-push fallback ────────────────────────────────────
#if __has_include(<generator>) && defined(__cpp_lib_generator) && __cpp_lib_generator >= 202207L
#  include <generator>
#  define MACRO_HAS_CORO 1
#else
#  define MACRO_HAS_CORO 0
namespace std {
template<typename Y>
struct generator {
    std::vector<Y> _v;
    void push(Y v)       { _v.push_back(std::move(v)); }
    auto begin()   const { return _v.begin(); }
    auto end()     const { return _v.end(); }
    bool empty()   const { return _v.empty(); }
};
} // namespace std
#endif

// ─── std::mdspan -> .at(r,c) 2D accessor ───────────────────────────────────────
#if __has_include(<mdspan>) && defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
#  include <mdspan>
#else
namespace std {
template<class T> struct dextents { using index_type = std::size_t; };
template<class T, class E>
class mdspan {
public:
    mdspan() = default;
    mdspan(T* p, std::size_t r, std::size_t c) : p_(p), r_(r), c_(c) {}
    T&  at(std::size_t r, std::size_t c)       noexcept { return p_[r*c_+c]; }
    T   at(std::size_t r, std::size_t c) const noexcept { return p_[r*c_+c]; }
    std::size_t extent(int d) const noexcept { return d==0 ? r_ : c_; }
private:
    T* p_{nullptr}; std::size_t r_{0}, c_{0};
};
} // namespace std
#endif

namespace macro {

// ─────────────────────── GeoSelectionContext ───────────────────────────────────
enum class GeoResolution : int {
    World = 0, Continent = 1, Country = 2, State = 3, City = 4, Ground = 5
};

[[nodiscard]] inline int fetch_tier(GeoResolution r) noexcept {
    switch (r) {
        case GeoResolution::World:     return 0;
        case GeoResolution::Continent: return 1;
        case GeoResolution::Country:   return 2;
        case GeoResolution::State:
        case GeoResolution::City:      return 3;
        case GeoResolution::Ground:    return 4; // satellite — suppress fetches
    }
    return 0;
}

struct GeoSelectionContext {
    GeoResolution resolution{GeoResolution::World};
    std::string continent, country_name, country_iso2, country_iso3;
    std::optional<std::string> admin1_name, city_name;
    double lat{0}, lon{0};
    std::array<double,4> bbox{-180.0,-90.0,180.0,90.0};
    bool is_g7{false}, is_g20{false}, is_eurozone{false}, is_eu{false},
         is_nato{false}, is_em{false};
    std::chrono::steady_clock::time_point settled_at{std::chrono::steady_clock::now()};

    [[nodiscard]] std::string breadcrumb() const {
        std::string bc = "World";
        if (resolution >= GeoResolution::Continent && !continent.empty())    bc += " / " + continent;
        if (resolution >= GeoResolution::Country && !country_name.empty())   bc += " / " + country_name;
        if (resolution >= GeoResolution::State && admin1_name)               bc += " / " + *admin1_name;
        if (resolution >= GeoResolution::City && city_name)                  bc += " / " + *city_name;
        if (resolution == GeoResolution::Ground)                             bc += " / Ground Intelligence";
        return bc;
    }

    [[nodiscard]] std::string selected_name() const {
        switch (resolution) {
            case GeoResolution::World:     return "World";
            case GeoResolution::Continent: return continent;
            case GeoResolution::Country:   return country_name;
            case GeoResolution::State:     return admin1_name.value_or(country_name);
            case GeoResolution::City:      return city_name.value_or(country_name);
            case GeoResolution::Ground:    return "Ground Intelligence";
        }
        return "Unknown";
    }

    [[nodiscard]] bool is_settled(int ms = 2000) const noexcept {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - settled_at).count();
        return age >= ms;
    }

    [[nodiscard]] std::string to_query_string() const {
        switch (resolution) {
            case GeoResolution::World:
                return "global economy OR macroeconomics OR central bank OR geopolitics OR inflation OR GDP OR interest rates OR trade";
            case GeoResolution::Continent:
                return continent + " economy OR " + continent + " markets OR " + continent + " central bank OR " + continent + " inflation";
            case GeoResolution::Country:
                if (!country_name.empty())
                    return country_name + " economy OR " + country_name + " inflation OR " + country_name +
                           " interest rates OR " + country_name + " central bank OR " + country_name + " GDP";
                return "global economy";
            case GeoResolution::State:
            case GeoResolution::City: {
                std::string loc = city_name ? *city_name : admin1_name.value_or(country_name);
                return loc + " economy OR " + loc + " business OR " + country_name + " regional economy";
            }
            case GeoResolution::Ground: return "";
        }
        return "global economy";
    }

    [[nodiscard]] std::string to_api_geo_param() const {
        if (resolution == GeoResolution::Country && !country_iso2.empty()) {
            std::string lc = country_iso2;
            std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
            return "&country=" + lc;
        }
        return "";
    }
};

// ─────────────────────── AppStateBus ───────────────────────────────────────────
class AppStateBus {
public:
    using Token    = std::size_t;
    using Callback = std::function<void(const GeoSelectionContext&)>;

    Token subscribe(Callback cb) {
        std::scoped_lock lk{sub_mtx_};
        Token tok = next_tok_++;
        subs_.emplace(tok, std::move(cb));
        return tok;
    }
    void unsubscribe(Token tok) { std::scoped_lock lk{sub_mtx_}; subs_.erase(tok); }

    void publish(const GeoSelectionContext& ctx) {
        { std::scoped_lock lk{q_mtx_}; q_.push(ctx); }
        dirty_.store(true, std::memory_order_release);
    }

    std::size_t dispatch_pending() {
        if (!dirty_.load(std::memory_order_acquire)) return 0;
        std::queue<GeoSelectionContext> local;
        { std::scoped_lock lk{q_mtx_}; std::swap(local, q_); dirty_.store(false, std::memory_order_release); }
        std::vector<Callback> cbs;
        { std::scoped_lock lk{sub_mtx_}; for (auto& kv : subs_) cbs.push_back(kv.second); }
        std::size_t n = 0;
        while (!local.empty()) { for (auto& cb : cbs) cb(local.front()); local.pop(); ++n; }
        return n;
    }

private:
    std::map<Token, Callback> subs_;
    std::mutex sub_mtx_;
    std::queue<GeoSelectionContext> q_;
    std::mutex q_mtx_;
    std::atomic<bool> dirty_{false};
    std::atomic<Token> next_tok_{1};
};

// ─────────────────────── Secrets ───────────────────────────────────────────────
struct Secrets {
    std::string fred_api_key, alpha_vantage_api_key, polygon_api_key;
    std::string marketstack_api_key, tradier_api_key, finnhub_api_key, axionquant_api_key;
    std::string newsapi_api_key, gnews_api_key, newsdataio_api_key, worldnewsapi_api_key;
    std::string anthropic_api_key, nasa_gibs_api_key;
    std::string mapbox_api_key, gee_project_id, gee_service_account_json;
};

struct MissingSecrets {
    std::vector<std::string> missing_vars, optional_missing;
};

[[nodiscard]] inline std::expected<Secrets, MissingSecrets> load_secrets() {
    Secrets s; MissingSecrets err;
    auto get = [&](const char* var, std::string& dst, bool req = true) {
        const char* v = std::getenv(var);
        if (v && v[0]) dst = v;
        else if (req)  err.missing_vars.emplace_back(var);
        else           err.optional_missing.emplace_back(var);
    };
    get("FRED_API_KEY",             s.fred_api_key);
    get("ALPHA_VANTAGE_API_KEY",    s.alpha_vantage_api_key);
    get("POLYGON_API_KEY",          s.polygon_api_key);
    get("MARKETSTACK_API_KEY",      s.marketstack_api_key);
    get("TRADIER_API_KEY",          s.tradier_api_key);
    get("FINNHUB_API_KEY",          s.finnhub_api_key);
    get("AXIONQUANT_API_KEY",       s.axionquant_api_key);
    get("NEWSAPI_API_KEY",          s.newsapi_api_key);
    get("GNEWS_API_KEY",            s.gnews_api_key);
    get("NEWSDATAIO_API_KEY",       s.newsdataio_api_key);
    get("WORLDNEWSAPI_API_KEY",     s.worldnewsapi_api_key);
    get("ANTHROPIC_API_KEY",        s.anthropic_api_key);
    get("NASA_GIBS_API_KEY",        s.nasa_gibs_api_key);
    get("MAPBOX_API_KEY",           s.mapbox_api_key,           false);
    get("GEE_PROJECT_ID",           s.gee_project_id,           false);
    get("GEE_SERVICE_ACCOUNT_JSON", s.gee_service_account_json, false);

    for (auto& v : err.missing_vars)     MACRO_LOG("MISSING key: %s", v.c_str());
    for (auto& v : err.optional_missing) MACRO_LOG("optional not set: %s", v.c_str());
    if (!err.missing_vars.empty()) return MACRO_UNEXPECTED(std::move(err));
    return s;
}

// ─────────────────────── IDataProvider ─────────────────────────────────────────
struct GeoTag {
    std::optional<std::string> continent, country_iso2, country_iso3, admin1, city;
    std::optional<double> lat, lon;
    [[nodiscard]] bool matches_country(std::string_view iso2) const noexcept {
        return country_iso2 && *country_iso2 == iso2;
    }
};

struct NormalizedRecord {
    std::string record_id, domain, source_name, headline, payload_json;
    GeoTag geo;
    int severity{0};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

enum class ProviderErrorKind { NetworkFailure, AuthFailure, ParseError, RateLimitExceeded, Unavailable, Unknown };

struct ProviderError {
    ProviderErrorKind kind{ProviderErrorKind::Unknown};
    std::string message;
    int http_status{0};
};

class IDataProvider {
public:
    virtual ~IDataProvider() = default;
    virtual std::expected<void, ProviderError> connect() = 0;
    virtual std::generator<NormalizedRecord>   poll()    = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::chrono::seconds poll_interval() const noexcept { return std::chrono::seconds{60}; }
};

// ─────────────────────── HttpClient ────────────────────────────────────────────
struct HttpResponse { int status_code{0}; std::string body; };

class HttpClient {
public:
    HttpClient()  { curl_global_init(CURL_GLOBAL_DEFAULT); handle_ = curl_easy_init(); }
    ~HttpClient() { if (handle_) curl_easy_cleanup(handle_); }
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    using Headers = std::vector<std::pair<std::string,std::string>>;

    [[nodiscard]] std::expected<HttpResponse, ProviderError>
    get(std::string_view url, const Headers& extra = {}) { return request(url, {}, extra); }

    [[nodiscard]] std::expected<HttpResponse, ProviderError>
    post(std::string_view url, std::string_view body, const Headers& extra = {}) { return request(url, body, extra); }

private:
    CURL* handle_{nullptr};

    static std::size_t write_cb(char* p, std::size_t sz, std::size_t nm, void* ud) {
        static_cast<std::string*>(ud)->append(p, sz*nm); return sz*nm;
    }

    [[nodiscard]] std::expected<HttpResponse, ProviderError>
    request(std::string_view url, std::string_view body, const Headers& extra) {
        if (!handle_) return MACRO_UNEXPECTED(ProviderError{ProviderErrorKind::Unavailable, "no curl handle"});
        curl_easy_reset(handle_);
        HttpResponse resp;
        std::string url_str{url};
        curl_easy_setopt(handle_, CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &resp.body);
        curl_easy_setopt(handle_, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle_, CURLOPT_USERAGENT, "MacroTerminal/0.4");

        curl_slist* hdrs = nullptr;
        if (!body.empty()) {
            hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
            std::string bstr{body};
            curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, bstr.c_str());
        }
        for (auto& kv : extra) hdrs = curl_slist_append(hdrs, (kv.first + ": " + kv.second).c_str());
        if (hdrs) curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, hdrs);

        CURLcode res = curl_easy_perform(handle_);
        if (hdrs) curl_slist_free_all(hdrs);
        if (res != CURLE_OK)
            return MACRO_UNEXPECTED(ProviderError{ProviderErrorKind::NetworkFailure, curl_easy_strerror(res)});

        long code = 0;
        curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &code);
        resp.status_code = static_cast<int>(code);
        if (code == 401 || code == 403)
            return MACRO_UNEXPECTED(ProviderError{ProviderErrorKind::AuthFailure, "HTTP " + std::to_string(code), (int)code});
        if (code == 429)
            return MACRO_UNEXPECTED(ProviderError{ProviderErrorKind::RateLimitExceeded, "Rate limited", 429});
        if (code >= 500)
            return MACRO_UNEXPECTED(ProviderError{ProviderErrorKind::Unavailable, "HTTP " + std::to_string(code), (int)code});
        return resp;
    }
};

// ─────────────────────── RecordQueue (MPSC) ────────────────────────────────────
class RecordQueue {
public:
    void push(NormalizedRecord r) { std::scoped_lock lk{mtx_}; q_.push(std::move(r)); }
    std::vector<NormalizedRecord> drain(int max = 400) {
        std::vector<NormalizedRecord> out; out.reserve((std::size_t)max);
        std::scoped_lock lk{mtx_};
        while (!q_.empty() && (int)out.size() < max) { out.push_back(std::move(q_.front())); q_.pop(); }
        return out;
    }
    std::size_t size() const { std::scoped_lock lk{mtx_}; return q_.size(); }
private:
    mutable std::mutex mtx_;
    std::queue<NormalizedRecord> q_;
};

} // namespace macro
