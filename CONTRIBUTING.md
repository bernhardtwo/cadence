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

### Trying the app against a test day

Debug builds read two environment variables so a test session never touches your real template or
progress files:

- `CADENCE_TEST_TEMPLATE`: path of a template JSON to load instead of the user's `week.json`.
- `CADENCE_TEST_DATA_DIR`: directory that replaces both the config and the data directory.

Release builds ignore both.

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

## Reporting issues

Open an issue with the platform, the Qt version and the steps to reproduce. For build problems include
the CMake configure output.
