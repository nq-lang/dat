#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// providers.hpp — FILE 2 of 4
// Macro Intelligence Terminal — Data Layer
//
// Consolidated from the prior multi-file layout. Contains:
//   • All 14 IDataProvider implementations (FRED, Finnhub, AlphaVantage,
//     Polygon, MarketStack, Tradier, AxionQuant, NewsAggregator, IMF,
//     WorldBank, OpenMeteo, WHO, NASA GIBS, GEE)
//   • ProviderEngine        — owns provider jthreads, feeds RecordQueue
//   • ArticleRecord / FeedDomain — narrative feed schema for Section 2
//   • GeoScopedFetcher      — on-demand geo-scoped article fetcher for
//                             Section 2's event-driven zoom-tier feeds
//
// Depends on core.hpp (included by the consuming .cpp before this file).
// ═══════════════════════════════════════════════════════════════════════════════
#include "core.hpp"
#include <nlohmann/json.hpp>
#include <ctime>
#include <unordered_map>

namespace macro {

// ═══════════════════════════════ PROVIDERS ═════════════════════════════════════

// ── FRED ─────────────────────────────────────────────────────────────────────
class FredProvider final : public IDataProvider {
public:
    explicit FredProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://api.stlouisfed.org/fred/series?series_id=FEDFUNDS&api_key=" + key_ + "&file_type=json");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[FRED] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        static constexpr const char* SERIES[] = {"FEDFUNDS","DGS10","CPIAUCSL","UNRATE","GDP","INDPRO","HOUST"};
        for (const char* sid : SERIES) {
            std::string url = "https://api.stlouisfed.org/fred/series/observations?series_id=" + std::string(sid) +
                              "&api_key=" + key_ + "&file_type=json&sort_order=desc&limit=2";
            auto r = http_.get(url);
            if (!r) continue;
            try {
                auto j = nlohmann::json::parse(r->body);
                auto& ob = j["observations"];
                if (!ob.is_array() || ob.empty()) continue;
                std::string val = ob[0].value("value","."), date = ob[0].value("date","");
                NormalizedRecord rec;
                rec.record_id = std::string("fred:") + sid;
                rec.domain = "econ_calendar"; rec.source_name = "FRED";
                rec.headline = std::string(sid) + ": " + val + " (" + date + ")";
                rec.payload_json = r->body.substr(0, 512);
                rec.geo.country_iso2 = "US"; rec.severity = 1;
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            } catch (...) {}
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "FRED"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{300}; }
private:
    std::string key_; HttpClient http_;
};

// ── Finnhub ──────────────────────────────────────────────────────────────────
class FinnhubProvider final : public IDataProvider {
public:
    explicit FinnhubProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://finnhub.io/api/v1/news?category=general&token=" + key_);
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[Finnhub] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        auto r = http_.get("https://finnhub.io/api/v1/news?category=general&token=" + key_);
        if (r) {
            try {
                auto j = nlohmann::json::parse(r->body);
                int n = 0;
                for (auto& a : j) {
                    if (n++ > 15) break;
                    std::string hl = a.value("headline",""); if (hl.empty()) continue;
                    NormalizedRecord rec;
                    rec.record_id = "finnhub:news:" + std::to_string(a.value("id",0));
                    rec.domain = "news"; rec.source_name = a.value("source","Finnhub");
                    rec.headline = hl; rec.payload_json = a.dump().substr(0,512);
                    rec.severity = 1;
                    rec.timestamp = std::chrono::system_clock::from_time_t((std::time_t)a.value("datetime",0LL));
                    gen.push(std::move(rec));
                }
            } catch (...) {}
        }
        auto r2 = http_.get("https://finnhub.io/api/v1/calendar/economic?token=" + key_);
        if (r2) {
            try {
                auto j = nlohmann::json::parse(r2->body);
                for (auto& it : j.value("economicCalendar", nlohmann::json::array())) {
                    std::string ev = it.value("event",""), cty = it.value("country","");
                    NormalizedRecord rec;
                    rec.record_id = "finnhub:econ:" + cty + ":" + ev;
                    rec.domain = "econ_calendar"; rec.source_name = "Finnhub";
                    rec.headline = "[" + cty + "] " + ev;
                    rec.payload_json = it.dump().substr(0,512);
                    rec.geo.country_iso2 = cty;
                    std::string imp = it.value("impact","low");
                    rec.severity = (imp=="high") ? 3 : (imp=="medium") ? 2 : 1;
                    rec.timestamp = std::chrono::system_clock::now();
                    gen.push(std::move(rec));
                }
            } catch (...) {}
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "Finnhub"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{60}; }
private:
    std::string key_; HttpClient http_;
};

// ── Alpha Vantage ────────────────────────────────────────────────────────────
class AlphaVantageProvider final : public IDataProvider {
public:
    explicit AlphaVantageProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=SPY&apikey=" + key_);
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[AlphaVantage] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        static constexpr const char* FNS[] = {"REAL_GDP","INFLATION","UNEMPLOYMENT","FEDERAL_FUNDS_RATE","CPI","RETAIL_SALES"};
        for (const char* fn : FNS) {
            auto r = http_.get(std::string("https://www.alphavantage.co/query?function=") + fn + "&apikey=" + key_);
            if (!r) continue;
            try {
                auto j = nlohmann::json::parse(r->body);
                if (!j.contains("data") || j["data"].empty()) continue;
                auto& d = j["data"][0];
                std::string val = d.value("value","."), date = d.value("date","");
                NormalizedRecord rec;
                rec.record_id = std::string("av:") + fn;
                rec.domain = "econ_calendar"; rec.source_name = "Alpha Vantage";
                rec.headline = std::string(fn) + ": " + val + " (" + date + ")";
                rec.payload_json = r->body.substr(0,512);
                rec.geo.country_iso2 = "US"; rec.severity = 1;
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            } catch (...) {}
        }
        static constexpr const char* ETFS[] = {"XLK","XLF","XLE","XLU","XLV","XLP","XLI","XLB","XLY","XLRE","XLC"};
        for (const char* e : ETFS) {
            auto r = http_.get(std::string("https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=") + e + "&apikey=" + key_);
            if (!r) continue;
            try {
                auto j = nlohmann::json::parse(r->body);
                NormalizedRecord rec;
                rec.record_id = std::string("av:quote:") + e;
                rec.domain = "sector_data"; rec.source_name = "Alpha Vantage";
                rec.headline = std::string(e) + " snapshot";
                rec.payload_json = j.dump().substr(0,512);
                rec.geo.country_iso2 = "US";
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            } catch (...) {}
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "Alpha Vantage"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{300}; }
private:
    std::string key_; HttpClient http_;
};

// ── Polygon.io ───────────────────────────────────────────────────────────────
class PolygonProvider final : public IDataProvider {
public:
    explicit PolygonProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://api.polygon.io/v2/aggs/ticker/SPY/prev?adjusted=true&apiKey=" + key_);
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[Polygon] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        auto r = http_.get("https://api.polygon.io/v2/reference/news?limit=20&order=desc&apiKey=" + key_);
        if (!r) return gen;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("results", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                NormalizedRecord rec;
                rec.record_id = "polygon:news:" + a.value("id","");
                rec.domain = "news"; rec.source_name = "Polygon.io";
                rec.headline = hl; rec.payload_json = a.dump().substr(0,512);
                rec.severity = 1; rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            }
        } catch (...) {}
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "Polygon.io"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{120}; }
private:
    std::string key_; HttpClient http_;
};

// ── MarketStack ──────────────────────────────────────────────────────────────
class MarketStackProvider final : public IDataProvider {
public:
    explicit MarketStackProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("http://api.marketstack.com/v1/eod?access_key=" + key_ + "&symbols=AAPL&limit=1");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[MarketStack] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        auto r = http_.get("http://api.marketstack.com/v1/eod?access_key=" + key_ +
                           "&symbols=SPY,QQQ,XLK,XLF,XLE,XLU,XLV,XLP,XLI,XLB,XLY,XLRE,XLC&limit=20&sort=DESC");
        if (!r) return gen;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& it : j.value("data", nlohmann::json::array())) {
                std::string sym = it.value("symbol","");
                NormalizedRecord rec;
                rec.record_id = "mstack:eod:" + sym;
                rec.domain = "sector_data"; rec.source_name = "MarketStack";
                rec.headline = sym + " close=" + std::to_string(it.value("close",0.0f));
                rec.payload_json = it.dump().substr(0,512);
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            }
        } catch (...) {}
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "MarketStack"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{300}; }
private:
    std::string key_; HttpClient http_;
};

// ── Tradier ──────────────────────────────────────────────────────────────────
class TradierProvider final : public IDataProvider {
public:
    explicit TradierProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://api.tradier.com/v1/markets/quotes?symbols=SPY",
                           {{"Authorization","Bearer "+key_},{"Accept","application/json"}});
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[Tradier] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        auto r = http_.get("https://api.tradier.com/v1/markets/quotes?symbols=SPY,XLK,XLF,XLE,XLU,XLV,XLP,XLI,XLB,XLY,XLRE,XLC",
                           {{"Authorization","Bearer "+key_},{"Accept","application/json"}});
        if (!r) return gen;
        try {
            auto j = nlohmann::json::parse(r->body);
            auto& qs = j["quotes"]["quote"];
            auto emit = [&](const nlohmann::json& q) {
                std::string sym = q.value("symbol","");
                float last = q.value("last", 0.0f), chg = q.value("change_percentage",0.0f);
                NormalizedRecord rec;
                rec.record_id = "tradier:quote:" + sym;
                rec.domain = "sector_data"; rec.source_name = "Tradier";
                rec.headline = sym + " last=" + std::to_string(last) + " chg=" + std::to_string(chg) + "%";
                rec.payload_json = q.dump().substr(0,512);
                rec.geo.country_iso2 = "US";
                rec.severity = (std::fabs(chg)>3.0f)?3:(std::fabs(chg)>1.5f)?2:1;
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            };
            if (qs.is_array()) for (auto& q : qs) emit(q);
            else if (qs.is_object()) emit(qs);
        } catch (...) {}
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "Tradier"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{120}; }
private:
    std::string key_; HttpClient http_;
};

// ── AxionQuant ───────────────────────────────────────────────────────────────
class AxionQuantProvider final : public IDataProvider {
public:
    explicit AxionQuantProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://api.axionquant.io/v1/health", {{"x-api-key", key_}});
        if (!r) { MACRO_LOG("[AxionQuant] health check failed - continuing anyway"); return {}; }
        MACRO_LOG("[AxionQuant] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        auto r = http_.get("https://api.axionquant.io/v1/signals/macro", {{"x-api-key", key_}});
        if (!r) return gen;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto it = j.begin(); it != j.end(); ++it) {
                NormalizedRecord rec;
                rec.record_id = "axq:signal:" + it.key();
                rec.domain = "sector_data"; rec.source_name = "AxionQuant";
                rec.headline = "AXQ " + it.key() + " signal: " +
                    (it.value().is_number() ? std::to_string(it.value().get<float>()) : it.value().dump());
                rec.payload_json = nlohmann::json{{it.key(), it.value()}}.dump();
                rec.geo.country_iso2 = "US"; rec.severity = 1;
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            }
        } catch (...) {}
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "AxionQuant"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{300}; }
private:
    std::string key_; HttpClient http_;
};

// ── News Aggregator (NewsAPI + GNews + NewsData.io + WorldNewsAPI) ───────────
class NewsAggregatorProvider final : public IDataProvider {
public:
    struct Config { std::string newsapi_key, gnews_key, newsdataio_key, worldnewsapi_key; };
    explicit NewsAggregatorProvider(Config c) : cfg_(std::move(c)) {}
    std::expected<void, ProviderError> connect() override {
        MACRO_LOG("[NewsAgg] configured with %s keys", cfg_.newsapi_key.empty() ? "partial" : "all");
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        fetch_newsapi(gen); fetch_gnews(gen); fetch_newsdataio(gen); fetch_worldnews(gen);
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "NewsAggregator"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{120}; }
private:
    Config cfg_; HttpClient http_;

    static int classify(const std::string& h) {
        std::string lo = h;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        if (lo.find("war")!=std::string::npos||lo.find("invasion")!=std::string::npos) return 5;
        if (lo.find("military")!=std::string::npos||lo.find("conflict")!=std::string::npos) return 4;
        if (lo.find("hike")!=std::string::npos||lo.find("inflation")!=std::string::npos) return 3;
        if (lo.find("gdp")!=std::string::npos||lo.find("central bank")!=std::string::npos) return 2;
        return 1;
    }
    void fetch_newsapi(std::generator<NormalizedRecord>& gen) {
        if (cfg_.newsapi_key.empty()) return;
        auto r = http_.get("https://newsapi.org/v2/top-headlines?category=business&pageSize=20&apiKey=" + cfg_.newsapi_key);
        if (!r || r->status_code != 200) return;
        try {
            auto j = nlohmann::json::parse(r->body);
            if (j.value("status","") != "ok") return;
            for (auto& a : j.value("articles", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                NormalizedRecord rec;
                rec.record_id = "newsapi:" + hl.substr(0,40);
                rec.domain = "news"; rec.source_name = a.contains("source") ? a["source"].value("name","NewsAPI") : "NewsAPI";
                rec.headline = hl; rec.payload_json = a.dump().substr(0,512);
                rec.severity = classify(hl); rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            }
        } catch (...) {}
    }
    void fetch_gnews(std::generator<NormalizedRecord>& gen) {
        if (cfg_.gnews_key.empty()) return;
        auto r = http_.get("https://gnews.io/api/v4/top-headlines?category=business&lang=en&max=10&token=" + cfg_.gnews_key);
        if (!r || r->status_code != 200) return;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("articles", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                NormalizedRecord rec;
                rec.record_id = "gnews:" + hl.substr(0,40);
                rec.domain = "news"; rec.source_name = "GNews";
                rec.headline = hl; rec.payload_json = a.dump().substr(0,512);
                rec.severity = classify(hl); rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            }
        } catch (...) {}
    }
    void fetch_newsdataio(std::generator<NormalizedRecord>& gen) {
        if (cfg_.newsdataio_key.empty()) return;
        auto r = http_.get("https://newsdata.io/api/1/news?category=business,politics&language=en&apikey=" + cfg_.newsdataio_key);
        if (!r || r->status_code != 200) return;
        try {
            auto j = nlohmann::json::parse(r->body);
            int n = 0;
            for (auto& a : j.value("results", nlohmann::json::array())) {
                if (n++ > 10) break;
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                NormalizedRecord rec;
                rec.record_id = "ndi:" + a.value("article_id","");
                rec.domain = "news"; rec.source_name = a.value("source_id","NewsData");
                rec.headline = hl; rec.payload_json = a.dump().substr(0,512);
                rec.severity = classify(hl); rec.timestamp = std::chrono::system_clock::now();
                if (a.contains("country") && a["country"].is_array() && !a["country"].empty())
                    rec.geo.country_iso2 = a["country"][0].get<std::string>();
                gen.push(std::move(rec));
            }
        } catch (...) {}
    }
    void fetch_worldnews(std::generator<NormalizedRecord>& gen) {
        if (cfg_.worldnewsapi_key.empty()) return;
        auto r = http_.get("https://api.worldnewsapi.com/search-news?text=economy+finance+central+bank&number=10&api-key=" + cfg_.worldnewsapi_key,
                           {{"x-api-key", cfg_.worldnewsapi_key}});
        if (!r || r->status_code != 200) return;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("news", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                NormalizedRecord rec;
                rec.record_id = "wna:" + std::to_string(a.value("id",0));
                rec.domain = "news"; rec.source_name = "WorldNewsAPI";
                rec.headline = hl; rec.payload_json = a.dump().substr(0,512);
                rec.severity = classify(hl); rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            }
        } catch (...) {}
    }
};

// ── IMF (no key) ─────────────────────────────────────────────────────────────
class IMFProvider final : public IDataProvider {
public:
    IMFProvider() = default;
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://dataservices.imf.org/REST/SDMX_JSON.svc/Dataflow");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[IMF] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        struct S { const char* db; const char* ind; const char* area; };
        static constexpr S SRS[] = {
            {"WEO","NGDP_RPCH","US"},{"WEO","NGDP_RPCH","DE"},{"WEO","NGDP_RPCH","JP"},
            {"WEO","PCPIPCH","US"},{"WEO","PCPIPCH","DE"},{"IFS","BCA_BP6_USD","US"},
        };
        for (const auto& s : SRS) {
            std::string url = std::string("https://dataservices.imf.org/REST/SDMX_JSON.svc/CompactData/") +
                              s.db + "/A." + s.ind + "." + s.area + "?startPeriod=2020&endPeriod=2025";
            auto r = http_.get(url);
            if (!r) continue;
            try {
                auto j = nlohmann::json::parse(r->body);
                auto& ds = j["CompactData"]["DataSet"];
                if (!ds.contains("Series")) continue;
                auto& obs = ds["Series"]["Obs"];
                if (!obs.is_array() || obs.empty()) continue;
                auto& lat = obs.back();
                std::string val = lat.value("@OBS_VALUE","—"), period = lat.value("@TIME_PERIOD","");
                NormalizedRecord rec;
                rec.record_id = std::string("imf:") + s.db + "." + s.ind + "." + s.area;
                rec.domain = std::string(s.db)=="WEO" ? "econ_calendar" : "monetary_policy";
                rec.source_name = "IMF";
                rec.headline = std::string("[IMF] ") + s.area + "/" + s.ind + ": " + val + " (" + period + ")";
                rec.payload_json = r->body.substr(0,512);
                rec.geo.country_iso2 = s.area; rec.severity = 1;
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            } catch (...) {}
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "IMF"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{3600}; }
private:
    HttpClient http_;
};

// ── World Bank (no key) ──────────────────────────────────────────────────────
class WorldBankProvider final : public IDataProvider {
public:
    WorldBankProvider() = default;
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://api.worldbank.org/v2/country/US/indicator/NY.GDP.MKTP.CD?format=json&mrv=1");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[WorldBank] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        struct I { const char* code; const char* label; const char* domain; };
        static constexpr I INDS[] = {
            {"NY.GDP.MKTP.KD.ZG","GDP growth (%)","econ_calendar"},
            {"FP.CPI.TOTL.ZG","CPI Inflation (%)","econ_calendar"},
            {"GC.DOD.TOTL.GD.ZS","Govt debt % GDP","monetary_policy"},
            {"PV.EST","Political Stability","geopolitics"},
        };
        static constexpr const char* CTRS[] = {"US","GB","DE","JP","CN","FR","IN","BR","CA","AU"};
        for (const auto& ind : INDS) {
            for (const char* iso : CTRS) {
                auto r = http_.get(std::string("https://api.worldbank.org/v2/country/") + iso +
                                   "/indicator/" + ind.code + "?format=json&mrv=2&per_page=2");
                if (!r) continue;
                try {
                    auto j = nlohmann::json::parse(r->body);
                    if (!j.is_array() || j.size()<2) continue;
                    auto& data = j[1];
                    if (!data.is_array() || data.empty()) continue;
                    auto& row = data[0];
                    if (row.is_null() || row.value("value",nullptr).is_null()) continue;
                    float val = row.value("value",0.0f);
                    NormalizedRecord rec;
                    rec.record_id = std::string("wb:") + iso + "." + ind.code;
                    rec.domain = ind.domain; rec.source_name = "World Bank";
                    rec.headline = std::string("[WB] ") + iso + " " + ind.label + ": " +
                                   std::to_string(val) + " (" + row.value("date","") + ")";
                    rec.payload_json = row.dump().substr(0,512);
                    rec.geo.country_iso2 = iso; rec.severity = 1;
                    rec.timestamp = std::chrono::system_clock::now();
                    gen.push(std::move(rec));
                } catch (...) {}
            }
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "World Bank"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{7200}; }
private:
    HttpClient http_;
};

// ── Open-Meteo (no key) ──────────────────────────────────────────────────────
class OpenMeteoProvider final : public IDataProvider {
public:
    OpenMeteoProvider() = default;
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current_weather=true&forecast_days=1");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[Open-Meteo] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        struct City { const char* n; const char* iso; double lat; double lon; };
        static constexpr City CITIES[] = {
            {"New York","US",40.71,-74.01},{"London","GB",51.51,-0.13},{"Tokyo","JP",35.68,139.69},
            {"Frankfurt","DE",50.11,8.68},{"Shanghai","CN",31.23,121.47},{"Dubai","AE",25.20,55.27},
        };
        for (const auto& c : CITIES) {
            std::string url = std::string("https://api.open-meteo.com/v1/forecast?latitude=") + std::to_string(c.lat) +
                              "&longitude=" + std::to_string(c.lon) + "&current_weather=true&forecast_days=1&timezone=auto";
            auto r = http_.get(url);
            if (!r) continue;
            try {
                auto j = nlohmann::json::parse(r->body);
                float temp = j.contains("current_weather") ? j["current_weather"].value("temperature", 0.0f) : 0.0f;
                NormalizedRecord rec;
                rec.record_id = std::string("meteo:") + c.n;
                rec.domain = "geopolitics"; rec.source_name = "Open-Meteo";
                rec.headline = std::string("[WEATHER] ") + c.n + " " + std::to_string(temp) + "C";
                rec.payload_json = r->body.substr(0,256);
                rec.geo.country_iso2 = c.iso; rec.geo.lat = c.lat; rec.geo.lon = c.lon; rec.severity = 0;
                rec.timestamp = std::chrono::system_clock::now();
                gen.push(std::move(rec));
            } catch (...) {}
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "Open-Meteo"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{900}; }
private:
    HttpClient http_;
};

// ── WHO GHO (no key) ──────────────────────────────────────────────────────────
class WHOProvider final : public IDataProvider {
public:
    WHOProvider() = default;
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://ghoapi.azureedge.net/api/Indicator?$top=1&$format=json");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[WHO] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        struct I { const char* code; const char* label; };
        static constexpr I INDS[] = {{"WHOSIS_000001","Life expectancy"},{"MDG_0000000026","U5 mortality"}};
        for (const auto& ind : INDS) {
            auto r = http_.get(std::string("https://ghoapi.azureedge.net/api/") + ind.code +
                               "?$top=5&$format=json&$filter=SpatialDim%20ne%20null&$orderby=TimeDim%20desc");
            if (!r) continue;
            try {
                auto j = nlohmann::json::parse(r->body);
                for (auto& row : j.value("value", nlohmann::json::array())) {
                    std::string cty = row.value("SpatialDim",""); if (cty.size()!=3) continue;
                    float val = row.value("NumericValue",0.0f);
                    NormalizedRecord rec;
                    rec.record_id = std::string("who:") + ind.code + "." + cty;
                    rec.domain = "geopolitics"; rec.source_name = "WHO GHO";
                    rec.headline = std::string("[WHO] ") + cty + " " + ind.label + ": " + std::to_string(val);
                    rec.payload_json = row.dump().substr(0,512);
                    rec.geo.country_iso3 = cty; rec.severity = 0;
                    rec.timestamp = std::chrono::system_clock::now();
                    gen.push(std::move(rec));
                }
            } catch (...) {}
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "WHO GHO"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{43200}; }
private:
    HttpClient http_;
};

// ── NASA GIBS (satellite metadata) ───────────────────────────────────────────
class NASAGIBSProvider final : public IDataProvider {
public:
    explicit NASAGIBSProvider(std::string k) : key_(std::move(k)) {}
    std::expected<void, ProviderError> connect() override {
        auto r = http_.get("https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/wmts.cgi?SERVICE=WMTS&REQUEST=GetCapabilities");
        if (!r) return MACRO_UNEXPECTED(r.error());
        MACRO_LOG("[NASA GIBS] connected HTTP %d", r->status_code);
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm gm{}; gmtime_r(&tt, &gm);
        char date[12]; std::strftime(date, sizeof(date), "%Y-%m-%d", &gm);
        struct Layer { const char* id; const char* label; };
        static constexpr Layer LAYERS[] = {
            {"VIIRS_SNPP_TrueColor_143m","VIIRS True Color (143m)"},
            {"FIRMS_VIIRS_SNPP_NRT","Active Fire Detections (VIIRS)"},
            {"MODIS_Terra_NDVI_8Day","MODIS NDVI (Vegetation)"},
        };
        for (const auto& l : LAYERS) {
            NormalizedRecord rec;
            rec.record_id = std::string("gibs:") + l.id;
            rec.domain = "geopolitics"; rec.source_name = "NASA GIBS";
            rec.headline = std::string("[GIBS] ") + l.label + " available for " + date;
            rec.payload_json = std::string("{\"layer\":\"") + l.id + "\",\"date\":\"" + date + "\"}";
            rec.severity = 0; rec.timestamp = now;
            gen.push(std::move(rec));
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "NASA GIBS"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{3600}; }
private:
    std::string key_; HttpClient http_;
};

// ── Google Earth Engine ───────────────────────────────────────────────────────
class GEEProvider final : public IDataProvider {
public:
    struct Config { std::string project_id, service_account_json; };
    explicit GEEProvider(Config c) : cfg_(std::move(c)) {}
    std::expected<void, ProviderError> connect() override {
        if (cfg_.project_id.empty() || cfg_.service_account_json.empty()) {
            MACRO_LOG("[GEE] credentials not set — stub mode");
            stub_ = true; return {};
        }
        MACRO_LOG("[GEE] project=%s", cfg_.project_id.c_str());
        return {};
    }
    std::generator<NormalizedRecord> poll() override {
        std::generator<NormalizedRecord> gen;
        struct S { const char* id; const char* label; };
        static constexpr S STUBS[] = {
            {"VIIRS_Nightlights","Nighttime Lights (VIIRS DNB)"},
            {"S5P_NO2","NO2 Industrial Emissions (Sentinel-5P)"},
        };
        for (const auto& s : STUBS) {
            NormalizedRecord rec;
            rec.record_id = std::string("gee:") + (stub_ ? "stub:" : "") + s.id;
            rec.domain = "geopolitics"; rec.source_name = stub_ ? "GEE (stub)" : "GEE";
            rec.headline = std::string("[GEE") + (stub_ ? " STUB] " : "] ") + s.label +
                           (stub_ ? " — auth pending" : " available");
            rec.payload_json = std::string("{\"layer\":\"") + s.id + "\",\"project\":\"" + cfg_.project_id + "\"}";
            rec.severity = 0; rec.timestamp = std::chrono::system_clock::now();
            gen.push(std::move(rec));
        }
        return gen;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "Google Earth Engine"; }
    [[nodiscard]] std::chrono::seconds poll_interval() const noexcept override { return std::chrono::seconds{86400}; }
private:
    Config cfg_; HttpClient http_; bool stub_{false};
};

// ═══════════════════════════════ PROVIDER ENGINE ═══════════════════════════════
class ProviderEngine {
public:
    explicit ProviderEngine(const Secrets& sec, RecordQueue& q) : q_(q) {
        add<FredProvider>(sec.fred_api_key);
        add<FinnhubProvider>(sec.finnhub_api_key);
        add<AlphaVantageProvider>(sec.alpha_vantage_api_key);
        add<PolygonProvider>(sec.polygon_api_key);
        add<MarketStackProvider>(sec.marketstack_api_key);
        add<TradierProvider>(sec.tradier_api_key);
        add<AxionQuantProvider>(sec.axionquant_api_key);
        add<NewsAggregatorProvider>(NewsAggregatorProvider::Config{
            sec.newsapi_api_key, sec.gnews_api_key, sec.newsdataio_api_key, sec.worldnewsapi_api_key});
        add<IMFProvider>();
        add<WorldBankProvider>();
        add<OpenMeteoProvider>();
        add<WHOProvider>();
        add<NASAGIBSProvider>(sec.nasa_gibs_api_key);
        add<GEEProvider>(GEEProvider::Config{sec.gee_project_id, sec.gee_service_account_json});
        MACRO_LOG("[ProviderEngine] %d providers registered", (int)provs_.size());
    }
    void start() {
        for (auto& p : provs_) {
            IDataProvider* raw = p.get();
            workers_.emplace_back([this, raw](std::stop_token st){ run(raw, st); });
        }
        MACRO_LOG("[ProviderEngine] all workers started");
    }
    void stop() {
        for (auto& w : workers_) w.request_stop();
        MACRO_LOG("[ProviderEngine] stop requested");
    }
    int total()   const { return (int)provs_.size(); }
    int healthy() const { return healthy_.load(); }
    int failed()  const { return failed_.load(); }
private:
    RecordQueue& q_;
    std::vector<std::unique_ptr<IDataProvider>> provs_;
    std::vector<std::jthread> workers_;
    std::atomic<int> healthy_{0}, failed_{0};

    template<typename T, typename... A>
    void add(A&&... a) { provs_.push_back(std::make_unique<T>(std::forward<A>(a)...)); }

    void run(IDataProvider* p, std::stop_token st) {
        MACRO_LOG("[%s] connecting...", p->name().data());
        auto conn = p->connect();
        if (!conn) {
            MACRO_LOG("[%s] FAILED: %s", p->name().data(), conn.error().message.c_str());
            failed_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        healthy_.fetch_add(1, std::memory_order_relaxed);
        while (!st.stop_requested()) {
            try {
                for (auto& rec : p->poll()) { if (st.stop_requested()) break; q_.push(rec); }
            } catch (const std::exception& e) {
                MACRO_LOG("[%s] poll exception: %s", p->name().data(), e.what());
            }
            auto deadline = std::chrono::steady_clock::now() + p->poll_interval();
            while (!st.stop_requested() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
        }
        MACRO_LOG("[%s] stopped", p->name().data());
    }
};

// ═══════════════════════════════ SECTION 2 DATA MODEL ══════════════════════════

enum class FeedDomain : int {
    MacroDevelopments=0, MicroDevelopments=1, GeopoliticalTensions=2,
    CentralBankUpdates=3, MonetaryPolicy=4, GlobalRegionalNews=5, MilitaryWarNews=6
};
static constexpr int FEED_DOMAIN_COUNT = 7;

[[nodiscard]] inline const char* feed_domain_label(FeedDomain d) noexcept {
    switch (d) {
        case FeedDomain::MacroDevelopments:    return "MACROECONOMIC DEVELOPMENTS";
        case FeedDomain::MicroDevelopments:    return "MICROECONOMIC DEVELOPMENTS";
        case FeedDomain::GeopoliticalTensions: return "GEOPOLITICAL & GEO-TENSIONS";
        case FeedDomain::CentralBankUpdates:   return "CENTRAL BANK UPDATES";
        case FeedDomain::MonetaryPolicy:       return "MONETARY POLICY";
        case FeedDomain::GlobalRegionalNews:   return "GLOBAL / REGIONAL NEWS";
        case FeedDomain::MilitaryWarNews:      return "MILITARY & WAR NEWS";
    }
    return "UNKNOWN";
}

struct MetricTag { std::string label, value; int severity{1}; };

struct ArticleRecord {
    std::string id;
    FeedDomain  domain;
    std::string source_name, source_url, headline, snippet;
    std::vector<MetricTag> metric_tags;
    int severity{0};
    std::chrono::system_clock::time_point published_at;
    std::string geo_label, fetch_tier_label;
    bool is_geo_fetched{false};
};

struct FetchResult {
    FeedDomain domain{FeedDomain::MacroDevelopments};
    std::vector<ArticleRecord> articles;
    std::string fetch_tier_label;
    bool is_loading{false}, is_error{false};
    std::string error_msg;
};

// ── GeoScopedFetcher — on-demand geo-scoped article fetcher for Section 2 ─────
class GeoScopedFetcher {
public:
    explicit GeoScopedFetcher(const Secrets& s) : sec_(s) {}
    ~GeoScopedFetcher() { stop(); }

    void start() { running_ = true; w_ = std::jthread([this](std::stop_token st){ run(st); }); }
    void stop()  { running_ = false; if (w_.joinable()) w_.request_stop(); }

    void request_fetch(const GeoSelectionContext& ctx) {
        std::scoped_lock l{req_m_}; pending_ = ctx;
        has_pending_.store(true, std::memory_order_release);
    }

    std::vector<FetchResult> drain_results() {
        std::vector<FetchResult> out;
        std::scoped_lock l{res_m_};
        while (!results_.empty()) { out.push_back(std::move(results_.front())); results_.pop(); }
        return out;
    }

    bool is_fetching() const noexcept { return busy_.load(std::memory_order_relaxed); }

private:
    Secrets sec_;
    std::jthread w_;
    std::atomic<bool> running_{false}, has_pending_{false}, busy_{false};
    std::mutex req_m_;
    std::optional<GeoSelectionContext> pending_;
    std::mutex res_m_;
    std::queue<FetchResult> results_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> rl_;
    std::mutex rl_m_;

    void run(std::stop_token st) {
        MACRO_LOG("[GeoFetcher] started");
        while (!st.stop_requested() && running_.load()) {
            if (!has_pending_.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds{40}); continue;
            }
            GeoSelectionContext ctx;
            {
                std::scoped_lock l{req_m_};
                if (!pending_) { has_pending_ = false; continue; }
                ctx = *pending_; pending_.reset();
                has_pending_.store(false, std::memory_order_release);
            }
            int tier = fetch_tier(ctx.resolution);
            if (tier == 4) continue;
            busy_ = true;
            std::string tl = tlbl(tier);
            for (int d = 0; d < FEED_DOMAIN_COUNT; ++d) push({FeedDomain(d),{},tl,true,false,""});
            using FD = FeedDomain;
            for (auto fd : {FD::MacroDevelopments, FD::MicroDevelopments, FD::GeopoliticalTensions,
                            FD::CentralBankUpdates, FD::MonetaryPolicy, FD::GlobalRegionalNews, FD::MilitaryWarNews}) {
                if (st.stop_requested()) break;
                fetch_domain(fd, ctx, tier, tl);
            }
            busy_ = false;
        }
        MACRO_LOG("[GeoFetcher] stopped");
    }

    void fetch_domain(FeedDomain fd, const GeoSelectionContext& ctx, int tier, const std::string& tl) {
        FetchResult r; r.domain = fd; r.fetch_tier_label = tl;
        try { r.articles = dispatch(fd, ctx, tier); }
        catch (const std::exception& e) { r.is_error = true; r.error_msg = e.what(); }
        push(std::move(r));
    }

    std::vector<ArticleRecord> dispatch(FeedDomain fd, const GeoSelectionContext& ctx, int tier) {
        using FD = FeedDomain;
        std::string q = scope_q(ctx, topic(fd));
        switch (fd) {
            case FD::MacroDevelopments:    return merge(newsapi(q,ctx,fd,tier,10), gnews(q,ctx,fd,tier,6));
            case FD::MicroDevelopments:    return merge(newsapi(q,ctx,fd,tier,10), polygon(ctx,fd,tier,6));
            case FD::GeopoliticalTensions: return merge(newsapi(q,ctx,fd,tier,10), worldnews(q,ctx,fd,tier,8));
            case FD::CentralBankUpdates:   return merge(newsapi(q,ctx,fd,tier,10), finnhub("forex",ctx,fd,tier,4));
            case FD::MonetaryPolicy: {
                auto v = newsapi(q,ctx,fd,tier,10);
                if (tier <= 2) { auto f = fred_rel(ctx,fd); v.insert(v.end(), f.begin(), f.end()); }
                return v;
            }
            case FD::GlobalRegionalNews: return merge(newsapi(q,ctx,fd,tier,10), newsdata(q,ctx,fd,tier,6));
            case FD::MilitaryWarNews:    return merge(worldnews(q,ctx,fd,tier,10), newsapi(q,ctx,fd,tier,6));
        }
        return {};
    }

    static std::string topic(FeedDomain fd) {
        switch (fd) {
            case FeedDomain::MacroDevelopments:    return "GDP OR inflation OR CPI OR employment OR NFP";
            case FeedDomain::MicroDevelopments:    return "corporate earnings OR revenue OR sector";
            case FeedDomain::GeopoliticalTensions: return "geopolitics OR sanctions OR diplomatic OR election";
            case FeedDomain::CentralBankUpdates:   return "Federal Reserve OR ECB OR central bank OR BOJ";
            case FeedDomain::MonetaryPolicy:       return "rate hike OR rate cut OR basis points OR QE";
            case FeedDomain::GlobalRegionalNews:   return "supply chain OR commodity OR energy prices OR trade";
            case FeedDomain::MilitaryWarNews:      return "military OR conflict OR war OR defense OR NATO";
        }
        return "economy";
    }
    static std::string scope_q(const GeoSelectionContext& ctx, const std::string& t) {
        if (ctx.resolution == GeoResolution::World) return t;
        return "(" + t + ") AND (" + ctx.to_query_string() + ")";
    }

    bool rl_ok(const std::string& k, std::chrono::seconds min = std::chrono::seconds{45}) {
        std::scoped_lock l{rl_m_};
        auto& t = rl_[k];
        if (std::chrono::steady_clock::now() - t < min) return false;
        t = std::chrono::steady_clock::now(); return true;
    }

    static std::string enc(const std::string& s) {
        std::string o; o.reserve(s.size()*2);
        for (unsigned char c : s) {
            if (std::isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') o += char(c);
            else if (c == ' ') o += "%20";
            else { char b[4]; std::snprintf(b,4,"%%%02X",c); o += b; }
        }
        return o;
    }

    static ArticleRecord mk(FeedDomain fd, const GeoSelectionContext& ctx, int tier,
        std::string src, std::string url, std::string hl, std::string snip,
        std::string id, int sev, std::chrono::system_clock::time_point ts) {
        ArticleRecord a;
        a.domain=fd; a.source_name=std::move(src); a.source_url=std::move(url);
        a.headline=std::move(hl); a.snippet=std::move(snip); a.id=std::move(id);
        a.geo_label=ctx.selected_name(); a.fetch_tier_label=tlbl(tier);
        a.is_geo_fetched=true; a.severity=sev; a.published_at=ts;
        a.metric_tags=extract_pills(a.headline + " " + a.snippet);
        return a;
    }

    std::vector<ArticleRecord> newsapi(const std::string& q, const GeoSelectionContext& ctx, FeedDomain fd, int tier, int mx) {
        if (sec_.newsapi_api_key.empty() || !rl_ok("newsapi")) return {};
        HttpClient h; std::string cp = ctx.to_api_geo_param();
        char url[1024];
        if (!cp.empty() && tier==2)
            std::snprintf(url,sizeof(url),"https://newsapi.org/v2/top-headlines?category=business%s&pageSize=%d&apiKey=%s",
                cp.c_str(), mx, sec_.newsapi_api_key.c_str());
        else
            std::snprintf(url,sizeof(url),"https://newsapi.org/v2/everything?q=%s&language=en&sortBy=publishedAt&pageSize=%d&apiKey=%s",
                enc(q.substr(0,450)).c_str(), mx, sec_.newsapi_api_key.c_str());
        auto r = h.get(url);
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out;
        try {
            auto j = nlohmann::json::parse(r->body);
            if (j.value("status","") != "ok") return out;
            for (auto& a : j.value("articles", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                std::string src = a.contains("source") ? a["source"].value("name","NewsAPI") : "NewsAPI";
                out.push_back(mk(fd,ctx,tier,src,a.value("url",""),hl,a.value("description",""),
                    std::to_string(std::hash<std::string>{}(a.value("url",""))), classify(hl), parse_ts(a.value("publishedAt",""))));
            }
        } catch (...) {}
        return out;
    }
    std::vector<ArticleRecord> gnews(const std::string& q, const GeoSelectionContext& ctx, FeedDomain fd, int tier, int mx) {
        if (sec_.gnews_api_key.empty() || !rl_ok("gnews", std::chrono::seconds{60})) return {};
        HttpClient h; char url[1024];
        std::snprintf(url,sizeof(url),"https://gnews.io/api/v4/search?q=%s&lang=en&max=%d&token=%s",
            enc(q.substr(0,280)).c_str(), mx, sec_.gnews_api_key.c_str());
        auto r = h.get(url);
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("articles", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                std::string src = a.contains("source") ? a["source"].value("name","GNews") : "GNews";
                out.push_back(mk(fd,ctx,tier,src,a.value("url",""),hl,a.value("description",""),
                    std::to_string(std::hash<std::string>{}(a.value("url",""))), classify(hl), parse_ts(a.value("publishedAt",""))));
            }
        } catch (...) {}
        return out;
    }
    std::vector<ArticleRecord> worldnews(const std::string& q, const GeoSelectionContext& ctx, FeedDomain fd, int tier, int mx) {
        if (sec_.worldnewsapi_api_key.empty() || !rl_ok("worldnews", std::chrono::seconds{60})) return {};
        HttpClient h; char url[1024];
        std::snprintf(url,sizeof(url),"https://api.worldnewsapi.com/search-news?text=%s&number=%d&api-key=%s",
            enc(q.substr(0,280)).c_str(), mx, sec_.worldnewsapi_api_key.c_str());
        auto r = h.get(url, {{"x-api-key", sec_.worldnewsapi_api_key}});
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("news", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                std::string snip = a.value("text","");
                if (snip.size() > 260) snip = snip.substr(0,260) + "…";
                out.push_back(mk(fd,ctx,tier,"WorldNewsAPI",a.value("url",""),hl,snip,
                    std::to_string(a.value("id",0)), classify(hl), parse_ts(a.value("publish_date",""))));
            }
        } catch (...) {}
        return out;
    }
    std::vector<ArticleRecord> newsdata(const std::string& q, const GeoSelectionContext& ctx, FeedDomain fd, int tier, int mx) {
        if (sec_.newsdataio_api_key.empty() || !rl_ok("newsdata", std::chrono::seconds{90})) return {};
        HttpClient h; std::string cp;
        if (tier==2 && !ctx.country_iso2.empty()) {
            std::string lc = ctx.country_iso2;
            std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
            cp = "&country=" + lc;
        }
        char url[1024];
        std::snprintf(url,sizeof(url),"https://newsdata.io/api/1/news?q=%s&language=en&category=business,politics%s&apikey=%s",
            enc(q.substr(0,180)).c_str(), cp.c_str(), sec_.newsdataio_api_key.c_str());
        auto r = h.get(url);
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out; int cnt=0;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("results", nlohmann::json::array())) {
                if (cnt++ >= mx) break;
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                out.push_back(mk(fd,ctx,tier,a.value("source_id","NewsData"),a.value("link",""),hl,
                    a.value("description",""), a.value("article_id",""), classify(hl), parse_ts(a.value("pubDate",""))));
            }
        } catch (...) {}
        return out;
    }
    std::vector<ArticleRecord> finnhub(const std::string& cat, const GeoSelectionContext& ctx, FeedDomain fd, int tier, int mx) {
        if (sec_.finnhub_api_key.empty() || !rl_ok("finnhub_n", std::chrono::seconds{60})) return {};
        HttpClient h; char url[512];
        std::snprintf(url,sizeof(url),"https://finnhub.io/api/v1/news?category=%s&token=%s", cat.c_str(), sec_.finnhub_api_key.c_str());
        auto r = h.get(url);
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out; int cnt=0;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j) {
                if (cnt++ >= mx) break;
                std::string hl = a.value("headline",""); if (hl.empty()) continue;
                std::string snip = a.value("summary","");
                if (snip.size() > 260) snip = snip.substr(0,260) + "…";
                out.push_back(mk(fd,ctx,tier,a.value("source","Finnhub"),a.value("url",""),hl,snip,
                    std::to_string(a.value("id",0)), classify(hl),
                    std::chrono::system_clock::from_time_t((std::time_t)a.value("datetime",0LL))));
            }
        } catch (...) {}
        return out;
    }
    std::vector<ArticleRecord> polygon(const GeoSelectionContext& ctx, FeedDomain fd, int tier, int mx) {
        if (sec_.polygon_api_key.empty() || !rl_ok("poly_n", std::chrono::seconds{60})) return {};
        HttpClient h; char url[512];
        std::snprintf(url,sizeof(url),"https://api.polygon.io/v2/reference/news?limit=%d&order=desc&apiKey=%s", mx, sec_.polygon_api_key.c_str());
        auto r = h.get(url);
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& a : j.value("results", nlohmann::json::array())) {
                std::string hl = a.value("title",""); if (hl.empty()) continue;
                out.push_back(mk(fd,ctx,tier,"Polygon.io",a.value("article_url",""),hl,a.value("description",""),
                    a.value("id",""), classify(hl), parse_ts(a.value("published_utc",""))));
            }
        } catch (...) {}
        return out;
    }
    std::vector<ArticleRecord> fred_rel(const GeoSelectionContext& ctx, FeedDomain fd) {
        if (sec_.fred_api_key.empty() || !rl_ok("fred_r", std::chrono::seconds{300})) return {};
        HttpClient h; char url[512];
        std::snprintf(url,sizeof(url),"https://api.stlouisfed.org/fred/releases?api_key=%s&file_type=json&limit=6&sort_order=desc", sec_.fred_api_key.c_str());
        auto r = h.get(url);
        if (!r || r->status_code != 200) return {};
        std::vector<ArticleRecord> out;
        try {
            auto j = nlohmann::json::parse(r->body);
            for (auto& rel : j.value("releases", nlohmann::json::array())) {
                std::string name = rel.value("name",""); if (name.empty()) continue;
                ArticleRecord a;
                a.domain=fd; a.source_name="FRED / St. Louis Fed"; a.headline="FRED Release: " + name;
                a.snippet = rel.value("notes","").substr(0, std::min(rel.value("notes","").size(), std::size_t{280}));
                a.id = "fred:rel:" + std::to_string(rel.value("id",0));
                a.geo_label=ctx.selected_name(); a.fetch_tier_label="COUNTRY";
                a.is_geo_fetched=true; a.severity=1;
                a.published_at=std::chrono::system_clock::now();
                out.push_back(std::move(a));
            }
        } catch (...) {}
        return out;
    }

    static int classify(const std::string& txt) {
        std::string lo = txt;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        if (lo.find("war")!=std::string::npos||lo.find("invasion")!=std::string::npos) return 5;
        if (lo.find("military")!=std::string::npos||lo.find("conflict")!=std::string::npos) return 4;
        if (lo.find("hike")!=std::string::npos||lo.find("inflation")!=std::string::npos) return 3;
        if (lo.find("gdp")!=std::string::npos||lo.find("central bank")!=std::string::npos) return 2;
        return 1;
    }
    static std::vector<MetricTag> extract_pills(const std::string& txt) {
        struct P { const char* kw; const char* lb; };
        static constexpr P PATS[] = {
            {"basis points","bps"},{"bps","bps"},{"inflation","CPI"},{"rate hike","Rate"},
            {"rate cut","Rate"},{"gdp","GDP"},{"nfp","NFP"},{"pmi","PMI"},{"yield","YLD"},
        };
        std::string lo = txt;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        std::vector<MetricTag> tags;
        for (const auto& p : PATS) {
            if (tags.size() >= 3) break;
            auto pos = lo.find(p.kw);
            if (pos == std::string::npos) continue;
            std::size_t st = pos > 20 ? pos - 20 : 0;
            std::string win = lo.substr(st, 40);
            for (std::size_t i = 0; i < win.size(); ++i) {
                if (!std::isdigit((unsigned char)win[i])) continue;
                std::size_t j = i;
                while (j < win.size() && (std::isdigit((unsigned char)win[j]) || win[j]=='.' || win[j]=='%' || win[j]=='-' || win[j]=='+')) ++j;
                std::string val = win.substr(i, j-i);
                if (!val.empty()) { MetricTag t; t.label=p.lb; t.value=val; t.severity=1; tags.push_back(t); }
                break;
            }
        }
        return tags;
    }
    static std::chrono::system_clock::time_point parse_ts(const std::string& s) {
        if (s.empty()) return std::chrono::system_clock::now();
        int y=2024,mo=1,d=1,h=0,mi=0,sc=0;
        std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y,&mo,&d,&h,&mi,&sc);
        std::tm tm{}; tm.tm_year=y-1900; tm.tm_mon=mo-1; tm.tm_mday=d; tm.tm_hour=h; tm.tm_min=mi; tm.tm_sec=sc;
        auto t = std::mktime(&tm);
        return t != -1 ? std::chrono::system_clock::from_time_t(t) : std::chrono::system_clock::now();
    }
    static std::string tlbl(int t) {
        switch (t) { case 0: return "GLOBAL"; case 1: return "CONTINENT"; case 2: return "COUNTRY"; case 3: return "LOCAL"; default: return "SATELLITE"; }
    }
    void push(FetchResult r) { std::scoped_lock l{res_m_}; results_.push(std::move(r)); }
    static std::vector<ArticleRecord> merge(std::vector<ArticleRecord> a, std::vector<ArticleRecord>&& b) {
        a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
        return a;
    }
};

} // namespace macro
