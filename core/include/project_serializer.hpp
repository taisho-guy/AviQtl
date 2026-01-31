#pragma once
#include <QString>

namespace Rina::UI {
class TimelineService;
class ProjectService;
} // namespace Rina::UI

namespace Rina::Core {
class ProjectSerializer {
  public:
    static bool save(const QString &path, const UI::TimelineService *timeline, const UI::ProjectService *project, QString *errorMessage = nullptr);
    static bool load(const QString &path, UI::TimelineService *timeline, UI::ProjectService *project, QString *errorMessage = nullptr);
};
} // namespace Rina::Core