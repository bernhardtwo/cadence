#pragma once

#include <cadence/core/template_json.hpp>

#include <QString>

#include <optional>

// Where the application keeps its files. Debug builds honor two environment variables so a test
// session never touches the real template or progress: CADENCE_TEST_TEMPLATE points at a template
// file and CADENCE_TEST_DATA_DIR replaces both the config and the data directory.
namespace apppaths {

QString configDir();
QString dataDir();
QString weekTemplatePath();
QString progressDir();

struct TemplateLoad {
    std::optional<cadence::core::TemplateDocument> document;
    QString error;
    QString path;
};

// Copies the bundled default template on first run, then parses and validates the user's copy.
TemplateLoad loadWeekTemplate();

} // namespace apppaths
