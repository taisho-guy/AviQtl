#pragma once
#include <QString>

namespace AviQtl::UI {
class TimelineService;
class ProjectService;
} // namespace AviQtl::UI

namespace AviQtl::Core {

class ProjectSerializer {
  public:
    static auto save(const QString &fileUrl, const UI::TimelineService *timeline, const UI::ProjectService *project, QString *errorMessage = nullptr) -> bool;
    static auto load(const QString &fileUrl, UI::TimelineService *timeline, UI::ProjectService *project, QString *errorMessage = nullptr) -> bool;
};

} // namespace AviQtl::Core
