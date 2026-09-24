# Contributing to Cadence

Thanks for helping. This document covers how to build, test and submit changes.

## Building and testing

Follow the platform instructions in `README.md`. For day to day work use the debug preset of your
platform, for example on Windows:

```bat
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

Before opening a pull request make sure:

- the build is warning free (the core library is compiled with `/W4` or `-Wall -Wextra -Wpedantic`
  and warnings are errors there),
- `ctest` passes,
- `cmake --build --preset <preset> --target all_qmllint` reports nothing for QML changes.

CI runs the release preset on Windows, Linux and macOS for every push and pull request.

### Before pushing from Windows: the core library under GCC

MSVC accepts a few things GCC and Clang reject (a missing field initializer under `-Wextra`, a
forward declaration inside a namespace), and Linux CI is where that surfaces. Before pushing,
compile the core library and its tests with GCC and the CI flags in WSL. No CMake or Ninja is
needed there: the sources are compiled directly, with the nlohmann JSON headers and the Catch2
amalgamated sources that the Windows configure step fetched under `build/windows-msvc-debug/_deps`.

```sh
REPO=/mnt/c/Users/<you>/projects/cadence
DEPS=$REPO/build/windows-msvc-debug/_deps
OUT=$HOME/cadence-gcc && mkdir -p $OUT/core $OUT/tests $OUT/include/catch2/matchers
for h in catch2/catch_test_macros.hpp catch2/matchers/catch_matchers_string.hpp; do
  echo '#include <catch_amalgamated.hpp>' > $OUT/include/$h
done
for f in $REPO/src/core/src/*.cpp; do
  g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror \
    -I$REPO/src/core/include -isystem $DEPS/nlohmann_json-src/include \
    -DCADENCE_VERSION_MAJOR=0 -DCADENCE_VERSION_MINOR=1 -DCADENCE_VERSION_PATCH=0 \
    -DCADENCE_VERSION_STRING='"0.1.0"' -c $f -o $OUT/core/$(basename $f .cpp).o
done
g++ -std=c++20 -O1 -c $DEPS/catch2-src/extras/catch_amalgamated.cpp -o $OUT/tests/catch_amalgamated.o
for f in $REPO/tests/core/*.cpp; do
  g++ -std=c++20 -I$REPO/src/core/include -isystem $OUT/include -isystem $DEPS/catch2-src/extras \
    -DCADENCE_EXPECTED_VERSION='"0.1.0"' -DCADENCE_RESOURCES_DIR="\"$REPO/resources\"" \
    -c $f -o $OUT/tests/$(basename $f .cpp).o
done
g++ $OUT/core/*.o $OUT/tests/*.o -o $OUT/cadence_core_tests && $OUT/cadence_core_tests
```

The version numbers are the ones in the root `CMakeLists.txt`. The test headers are included by
their per-header Catch2 names, so two one-line shims forward them to the amalgamated header. The
core library must compile without a single warning and every test must pass before the push.

### Trying the app against a test day

Debug builds read two environment variables so a test session never touches your real template or
progress files:

- `CADENCE_TEST_TEMPLATE`: path of a template JSON to load instead of the user's `week.json`.
- `CADENCE_TEST_DATA_DIR`: directory that replaces both the config and the data directory.

A test session also uses its own single instance key and a no-op launch at login registration, so it
never wakes or reconfigures the copy you actually use. Release builds ignore both variables. A
`--profile` option that works in every build is planned to replace them; see `docs/follow-ups.md`.

## Commit messages

Cadence uses [Conventional Commits](https://www.conventionalcommits.org/). The subject line is
`type(scope): summary`, written in the imperative and without a trailing period. Scopes are the top
level areas of the tree: `core`, `app`, `theme`, `tests`, `ci`, `docs`. Common types:

| Type | Use for |
| --- | --- |
| `feat` | user visible behavior |
| `fix` | bug fixes |
| `refactor` | changes that neither fix nor add behavior |
| `test` | tests only |
| `docs` | documentation only |
| `ci` | workflow changes |
| `chore` | build system, tooling, dependencies |

Keep each commit focused on one logical change and explain the reasoning in the body when the subject
is not enough. Do not amend or force push commits that are already on `main`.

## Code style

C++ is formatted with `clang-format` using the `.clang-format` at the repository root (LLVM base,
4 space indent, 110 columns). Run it on the files you touch before committing.

- C++20, no compiler extensions.
- `src/core` must not depend on Qt. Put domain logic there and keep the app layer thin.
- Prefer `enum class`, `std::string_view` and value types in core. Use Qt types only in `src/app`.
- Comment only decisions that are not obvious from the code. Do not restate what the code does.
- No em dashes in code, comments or documentation.

QML follows the same spirit:

- Every color, font, size and spacing comes from the `Theme` singleton. No literals in components.
- Only the Basic control style is allowed. Material, Fusion, Universal and Imagine must not be imported
  and every control we render gets its own visual override.
- Name ids with lowerCamelCase and keep one component per file.
- Every changing time value (countdowns, remaining time, the summary clock) renders through
  `TimerText`, never a plain `Text`. Big Shoulders Display has no tabular figures, so the component
  lays each digit out in a fixed cell to keep the digits from shifting as they count.
- Every user-facing string is marked for translation: `qsTr()` in QML, `tr()` in C++, `arg()`
  placeholders instead of concatenation and `%n` for counts. User content (block, template,
  activity and playlist names) is never translated, and strings QML compares against stay English
  keys next to their translated labels. See `docs/i18n.md` for the lupdate and lrelease flow.
- Quotes render through `QuoteBlock` only, on the surfaces listed in `docs/quotes.md`.

## Reporting issues

Open an issue with the platform, the Qt version and the steps to reproduce. For build problems include
the CMake configure output.
