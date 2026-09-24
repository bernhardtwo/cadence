#include <cadence/core/quotes.hpp>

#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>

namespace cadence::core {

namespace {

using Json = nlohmann::ordered_json;

constexpr std::array<std::pair<QuoteContext, std::string_view>, quoteContextCount> contextNames{{
    {QuoteContext::BlockStart, "blockStart"},
    {QuoteContext::Focus, "focus"},
    {QuoteContext::Break, "break"},
    {QuoteContext::Pushups, "pushups"},
    {QuoteContext::BlockEnd, "blockEnd"},
}};

[[noreturn]] void fail(const std::string& where, const std::string& message) {
    throw QuoteError(where + ": " + message);
}

std::string requireString(const Json& object, const char* key, const std::string& where) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string() || it->get<std::string>().empty()) {
        fail(where, std::string("missing or empty \"") + key + "\"");
    }
    return it->get<std::string>();
}

Localized readLocalized(const Json& value, const std::string& where) {
    if (!value.is_object()) {
        fail(where, "expected an object with en, es and fr");
    }
    return Localized{requireString(value, "en", where), requireString(value, "es", where),
                     requireString(value, "fr", where)};
}

std::map<std::string, Localized> readLocalizedMap(const Json& root, const char* key) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_object() || it->empty()) {
        fail(key, "expected a non empty object");
    }
    std::map<std::string, Localized> out;
    for (const auto& [name, value] : it->items()) {
        out[name] = readLocalized(value, std::string(key) + "." + name);
    }
    return out;
}

} // namespace

std::string_view quoteContextName(QuoteContext context) noexcept {
    for (const auto& [value, name] : contextNames) {
        if (value == context) {
            return name;
        }
    }
    return {};
}

std::optional<QuoteContext> parseQuoteContext(std::string_view name) noexcept {
    for (const auto& [value, contextName] : contextNames) {
        if (contextName == name) {
            return value;
        }
    }
    return std::nullopt;
}

const std::string& Localized::in(std::string_view language) const noexcept {
    if (language == "es") {
        return es;
    }
    if (language == "fr") {
        return fr;
    }
    return en;
}

const Quote* QuoteCatalog::find(std::string_view id) const noexcept {
    const auto it =
        std::find_if(quotes.begin(), quotes.end(), [id](const Quote& quote) { return quote.id == id; });
    return it == quotes.end() ? nullptr : &*it;
}

std::vector<std::string> QuoteCatalog::idsIn(QuoteContext context) const {
    std::vector<std::string> ids;
    for (const Quote& quote : quotes) {
        if (quote.context == context) {
            ids.push_back(quote.id);
        }
    }
    return ids;
}

std::string QuoteCatalog::attribution(const Quote& quote, std::string_view language) const {
    const auto author = authors.find(quote.author);
    const auto work = works.find(quote.work);
    std::string text = author == authors.end() ? quote.author : author->second.in(language);
    const std::string workName = work == works.end() ? quote.work : work->second.in(language);
    if (quote.citing) {
        // "Marcus Aurelius, citing Democritus · Meditations 4.24": the comma already separates,
        // so the locus follows the work without one.
        text += ", " + quote.citing->in(language) + " · " + workName + " " + quote.locus;
    } else {
        text += " · " + workName + ", " + quote.locus;
    }
    return text;
}

QuoteCatalog parseQuoteCatalog(std::string_view json) {
    Json root;
    try {
        root = Json::parse(json);
    } catch (const Json::parse_error& error) {
        throw QuoteError(std::string("quotes are not valid JSON: ") + error.what());
    }
    if (!root.is_object()) {
        fail("$", "expected an object");
    }
    const auto version = root.find("version");
    if (version == root.end() || !version->is_number_integer() || version->get<int>() != 1) {
        fail("version", "expected 1");
    }

    QuoteCatalog catalog;
    catalog.authors = readLocalizedMap(root, "authors");
    catalog.works = readLocalizedMap(root, "works");

    const auto list = root.find("quotes");
    if (list == root.end() || !list->is_array() || list->empty()) {
        fail("quotes", "expected a non empty array");
    }
    std::set<std::string> ids;
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& item = (*list)[i];
        const std::string where = "quotes[" + std::to_string(i) + "]";
        if (!item.is_object()) {
            fail(where, "expected an object");
        }
        Quote quote;
        quote.id = requireString(item, "id", where);
        if (!ids.insert(quote.id).second) {
            fail(where, "duplicate id \"" + quote.id + "\"");
        }
        const std::string contextName = requireString(item, "context", where);
        const auto context = parseQuoteContext(contextName);
        if (!context) {
            fail(where, "unknown context \"" + contextName + "\"");
        }
        quote.context = *context;
        quote.author = requireString(item, "author", where);
        if (!catalog.authors.contains(quote.author)) {
            fail(where, "unknown author \"" + quote.author + "\"");
        }
        quote.work = requireString(item, "work", where);
        if (!catalog.works.contains(quote.work)) {
            fail(where, "unknown work \"" + quote.work + "\"");
        }
        quote.locus = requireString(item, "locus", where);
        quote.language = requireString(item, "lang", where);
        if (quote.language != "la" && quote.language != "grc") {
            fail(where, "lang must be la or grc");
        }
        quote.original = requireString(item, "original", where);
        quote.text = Localized{requireString(item, "en", where), requireString(item, "es", where),
                               requireString(item, "fr", where)};
        if (const auto citing = item.find("citing"); citing != item.end() && !citing->is_null()) {
            quote.citing = readLocalized(*citing, where + ".citing");
        }
        catalog.quotes.push_back(std::move(quote));
    }
    for (const auto& [context, name] : contextNames) {
        if (catalog.idsIn(context).empty()) {
            fail("quotes", "no quote for context \"" + std::string(name) + "\"");
        }
    }
    return catalog;
}

ShuffleBag::ShuffleBag(std::vector<std::string> ids, const State& saved, std::uint64_t seed)
    : ids_(std::move(ids)), random_(seed) {
    // Only ids the group still has survive a restore; a stale last id is forgotten too.
    for (const std::string& id : saved.remaining) {
        if (std::find(ids_.begin(), ids_.end(), id) != ids_.end() &&
            std::find(state_.remaining.begin(), state_.remaining.end(), id) == state_.remaining.end()) {
            state_.remaining.push_back(id);
        }
    }
    if (std::find(ids_.begin(), ids_.end(), saved.last) != ids_.end()) {
        state_.last = saved.last;
    }
}

void ShuffleBag::refill() {
    state_.remaining = ids_;
    std::shuffle(state_.remaining.begin(), state_.remaining.end(), random_);
    // The next draw is the back; it must not repeat what the previous round ended with.
    if (state_.remaining.size() > 1 && state_.remaining.back() == state_.last) {
        std::swap(state_.remaining.back(), state_.remaining.front());
    }
}

std::string ShuffleBag::next() {
    if (ids_.empty()) {
        return {};
    }
    if (state_.remaining.empty()) {
        refill();
    }
    std::string id = state_.remaining.back();
    state_.remaining.pop_back();
    state_.last = id;
    return id;
}

std::string serializeBagStates(const BagStates& states) {
    Json root;
    root["version"] = 1;
    Json contexts = Json::object();
    for (const auto& [context, state] : states) {
        Json entry;
        entry["remaining"] = state.remaining;
        entry["last"] = state.last;
        contexts[context] = std::move(entry);
    }
    root["contexts"] = std::move(contexts);
    return root.dump(2) + "\n";
}

BagStates parseBagStates(std::string_view json) noexcept {
    BagStates states;
    Json root = Json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return states;
    }
    const auto contexts = root.find("contexts");
    if (contexts == root.end() || !contexts->is_object()) {
        return states;
    }
    for (const auto& [context, entry] : contexts->items()) {
        if (!entry.is_object()) {
            continue;
        }
        ShuffleBag::State state;
        if (const auto remaining = entry.find("remaining");
            remaining != entry.end() && remaining->is_array()) {
            for (const Json& id : *remaining) {
                if (id.is_string()) {
                    state.remaining.push_back(id.get<std::string>());
                }
            }
        }
        if (const auto last = entry.find("last"); last != entry.end() && last->is_string()) {
            state.last = last->get<std::string>();
        }
        states[context] = std::move(state);
    }
    return states;
}

} // namespace cadence::core
