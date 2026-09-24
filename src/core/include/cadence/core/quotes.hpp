#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cadence::core {

// Where a quote is shown. Each surface draws from its own group.
enum class QuoteContext {
    BlockStart,
    Focus,
    Break,
    Pushups,
    BlockEnd,
};

inline constexpr int quoteContextCount = 5;

std::string_view quoteContextName(QuoteContext context) noexcept;
std::optional<QuoteContext> parseQuoteContext(std::string_view name) noexcept;

// A string in the three UI languages. in() falls back to English for anything else.
struct Localized {
    std::string en;
    std::string es;
    std::string fr;

    const std::string& in(std::string_view language) const noexcept;

    friend bool operator==(const Localized&, const Localized&) = default;
};

struct Quote {
    std::string id;
    QuoteContext context = QuoteContext::Focus;
    // Keys into QuoteCatalog::authors and works.
    std::string author;
    std::string work;
    std::string locus;
    std::optional<Localized> citing;
    // "la" or "grc": the language of the original line.
    std::string language;
    std::string original;
    Localized text;

    friend bool operator==(const Quote&, const Quote&) = default;
};

class QuoteError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct QuoteCatalog {
    std::map<std::string, Localized> authors;
    std::map<std::string, Localized> works;
    std::vector<Quote> quotes;

    const Quote* find(std::string_view id) const noexcept;
    std::vector<std::string> idsIn(QuoteContext context) const;

    // "Author · Work, locus", or "Author, citing X · Work locus" when the quote cites someone.
    // Mixed case: the UI uppercases where the design asks for it.
    std::string attribution(const Quote& quote, std::string_view language) const;
};

// Parses and validates the quotes resource. Throws QuoteError on the first problem found: a
// missing field, an unknown context, an author or work key that does not resolve, a duplicate id
// or a context without quotes.
QuoteCatalog parseQuoteCatalog(std::string_view json);

// Draws every id of a group once before any repeats, and never opens a new round with the id
// that closed the previous one.
class ShuffleBag {
public:
    struct State {
        // Ids still to draw in this round, in draw order (the back is next).
        std::vector<std::string> remaining;
        std::string last;

        friend bool operator==(const State&, const State&) = default;
    };

    // ids is the whole group. Ids in the saved state that the group no longer has are dropped,
    // and an empty or foreign state starts a fresh round.
    ShuffleBag(std::vector<std::string> ids, const State& saved, std::uint64_t seed);

    std::string next();
    const State& state() const noexcept { return state_; }
    std::size_t size() const noexcept { return ids_.size(); }

private:
    void refill();

    std::vector<std::string> ids_;
    State state_;
    std::mt19937_64 random_;
};

// The bag state of every context, as persisted between runs.
using BagStates = std::map<std::string, ShuffleBag::State>;

std::string serializeBagStates(const BagStates& states);
// Lenient: anything unreadable yields no state for that context, never an exception.
BagStates parseBagStates(std::string_view json) noexcept;

} // namespace cadence::core
