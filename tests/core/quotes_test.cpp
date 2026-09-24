#include <cadence/core/quotes.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace cadence::core;
using Catch::Matchers::ContainsSubstring;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

const std::string& shippedQuotes() {
    static const std::string text = readFile(std::string(CADENCE_RESOURCES_DIR) + "/quotes/quotes.json");
    return text;
}

// A tiny valid catalog; tests break one thing at a time.
const char* const minimal = R"({
  "version": 1,
  "authors": {"a": {"en": "Author", "es": "Autor", "fr": "Auteur"}},
  "works": {"w": {"en": "Work", "es": "Obra", "fr": "Oeuvre"}},
  "quotes": [
    {"id": "q1", "context": "blockStart", "author": "a", "work": "w", "locus": "1", "lang": "la", "original": "o", "en": "e", "es": "s", "fr": "f"},
    {"id": "q2", "context": "focus", "author": "a", "work": "w", "locus": "2", "lang": "la", "original": "o", "en": "e", "es": "s", "fr": "f"},
    {"id": "q3", "context": "break", "author": "a", "work": "w", "locus": "3", "lang": "grc", "original": "o", "en": "e", "es": "s", "fr": "f"},
    {"id": "q4", "context": "pushups", "author": "a", "work": "w", "locus": "4", "lang": "la", "original": "o", "en": "e", "es": "s", "fr": "f"},
    {"id": "q5", "context": "blockEnd", "author": "a", "work": "w", "locus": "5", "lang": "la", "original": "o", "en": "e", "es": "s", "fr": "f",
     "citing": {"en": "citing B", "es": "citando a B", "fr": "citant B"}}
  ]
})";

std::string withReplacement(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

} // namespace

TEST_CASE("the shipped quotes load and every quote is complete", "[quotes]") {
    const QuoteCatalog catalog = parseQuoteCatalog(shippedQuotes());
    CHECK(catalog.quotes.size() == 15);
    std::set<std::string> ids;
    for (const Quote& quote : catalog.quotes) {
        CHECK(ids.insert(quote.id).second);
        CHECK_FALSE(quote.locus.empty());
        CHECK_FALSE(quote.original.empty());
        CHECK_FALSE(quote.text.en.empty());
        CHECK_FALSE(quote.text.es.empty());
        CHECK_FALSE(quote.text.fr.empty());
        CHECK(catalog.authors.contains(quote.author));
        CHECK(catalog.works.contains(quote.work));
        CHECK((quote.language == "la" || quote.language == "grc"));
    }
    for (const QuoteContext context : {QuoteContext::BlockStart, QuoteContext::Focus, QuoteContext::Break,
                                       QuoteContext::Pushups, QuoteContext::BlockEnd}) {
        CHECK_FALSE(catalog.idsIn(context).empty());
    }
    // French typography uses U+00A0, never U+202F.
    CHECK(shippedQuotes().find("\xE2\x80\xAF") == std::string::npos);
}

TEST_CASE("attribution names author, work and locus in the language", "[quotes]") {
    const QuoteCatalog catalog = parseQuoteCatalog(shippedQuotes());
    const Quote* seneca = catalog.find("seneca-ep-2");
    REQUIRE(seneca != nullptr);
    CHECK(catalog.attribution(*seneca, "es") == "S\xC3\xA9neca \xC2\xB7 Cartas a Lucilio, 2");
    CHECK(catalog.attribution(*seneca, "en") == "Seneca \xC2\xB7 Letters to Lucilius, 2");
    const Quote* marcus = catalog.find("marcus-4-24");
    REQUIRE(marcus != nullptr);
    CHECK(catalog.attribution(*marcus, "es") == "Marco Aurelio, citando a Dem\xC3\xB3"
                                                "crito \xC2\xB7 Meditaciones 4.24");
    CHECK(catalog.attribution(*marcus, "de") ==
          "Marcus Aurelius, citing Democritus \xC2\xB7 Meditations 4.24");
    CHECK(catalog.find("nope") == nullptr);
}

TEST_CASE("catalog validation names the problem", "[quotes]") {
    CHECK_NOTHROW(parseQuoteCatalog(minimal));
    CHECK_THROWS_WITH(parseQuoteCatalog("nope"), ContainsSubstring("not valid JSON"));
    CHECK_THROWS_WITH(parseQuoteCatalog(withReplacement(minimal, "\"version\": 1", "\"version\": 2")),
                      ContainsSubstring("version"));
    CHECK_THROWS_WITH(parseQuoteCatalog(withReplacement(minimal, "\"id\": \"q2\"", "\"id\": \"q1\"")),
                      ContainsSubstring("duplicate id"));
    CHECK_THROWS_WITH(
        parseQuoteCatalog(withReplacement(minimal, "\"context\": \"focus\"", "\"context\": \"lunch\"")),
        ContainsSubstring("unknown context"));
    CHECK_THROWS_WITH(
        parseQuoteCatalog(withReplacement(minimal, "\"context\": \"focus\"", "\"context\": \"break\"")),
        ContainsSubstring("no quote for context \"focus\""));
    CHECK_THROWS_WITH(
        parseQuoteCatalog(withReplacement(minimal, "\"author\": \"a\", \"work\": \"w\", \"locus\": \"3\"",
                                          "\"author\": \"x\", \"work\": \"w\", \"locus\": \"3\"")),
        ContainsSubstring("unknown author"));
    CHECK_THROWS_WITH(parseQuoteCatalog(withReplacement(minimal, "\"work\": \"w\", \"locus\": \"4\"",
                                                        "\"work\": \"z\", \"locus\": \"4\"")),
                      ContainsSubstring("unknown work"));
    CHECK_THROWS_WITH(parseQuoteCatalog(withReplacement(minimal, "\"locus\": \"5\"", "\"locus\": \"\"")),
                      ContainsSubstring("locus"));
    CHECK_THROWS_WITH(parseQuoteCatalog(withReplacement(minimal, "\"lang\": \"grc\"", "\"lang\": \"el\"")),
                      ContainsSubstring("lang"));
    CHECK_THROWS_WITH(parseQuoteCatalog(withReplacement(minimal, "\"fr\": \"f\"}", "\"fr\": \"\"}")),
                      ContainsSubstring("fr"));
}

TEST_CASE("a shuffle bag draws every id once per round", "[quotes]") {
    const std::vector<std::string> ids{"a", "b", "c", "d", "e"};
    ShuffleBag bag(ids, {}, 42);
    for (int round = 0; round < 20; ++round) {
        std::set<std::string> seen;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            seen.insert(bag.next());
        }
        CHECK(seen.size() == ids.size());
    }
}

TEST_CASE("a new round never starts with the id that closed the last", "[quotes]") {
    const std::vector<std::string> ids{"a", "b", "c"};
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        ShuffleBag bag(ids, {}, seed);
        std::string previous = bag.next();
        for (int i = 0; i < 30; ++i) {
            const std::string current = bag.next();
            CHECK(current != previous);
            previous = current;
        }
    }
    // A group of one has no choice.
    ShuffleBag single({"only"}, {}, 1);
    CHECK(single.next() == "only");
    CHECK(single.next() == "only");
    ShuffleBag empty({}, {}, 1);
    CHECK(empty.next().empty());
}

TEST_CASE("bag state persists and restores mid round", "[quotes]") {
    const std::vector<std::string> ids{"a", "b", "c", "d"};
    ShuffleBag first(ids, {}, 7);
    const std::string drawn1 = first.next();
    const std::string drawn2 = first.next();
    const BagStates saved{{"focus", first.state()}};
    const std::string json = serializeBagStates(saved);
    CHECK_THAT(json, ContainsSubstring("\"remaining\""));

    const BagStates restored = parseBagStates(json);
    REQUIRE(restored.contains("focus"));
    CHECK(restored.at("focus") == first.state());
    ShuffleBag second(ids, restored.at("focus"), 99);
    // The two remaining ids come out before anything repeats.
    std::set<std::string> rest{second.next(), second.next()};
    CHECK(rest.size() == 2);
    CHECK_FALSE(rest.contains(drawn1));
    CHECK_FALSE(rest.contains(drawn2));
}

TEST_CASE("unknown or removed ids in a saved state are dropped", "[quotes]") {
    ShuffleBag::State saved;
    saved.remaining = {"gone", "b", "b", "also-gone"};
    saved.last = "vanished";
    ShuffleBag bag({"a", "b", "c"}, saved, 3);
    CHECK(bag.state().remaining == std::vector<std::string>{"b"});
    CHECK(bag.state().last.empty());
    CHECK(bag.next() == "b");
    // The next round is a full fresh one.
    std::set<std::string> round{bag.next(), bag.next(), bag.next()};
    CHECK(round.size() == 3);
}

TEST_CASE("unreadable bag state yields nothing instead of failing", "[quotes]") {
    CHECK(parseBagStates("not json").empty());
    CHECK(parseBagStates("[]").empty());
    CHECK(
        parseBagStates(R"({"contexts": {"focus": 5, "break": {"remaining": [1, "x"], "last": 2}}})").size() ==
        1);
    const BagStates states = parseBagStates(R"({"contexts": {"break": {"remaining": [1, "x"], "last": 2}}})");
    CHECK(states.at("break").remaining == std::vector<std::string>{"x"});
    CHECK(states.at("break").last.empty());
}
