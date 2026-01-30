#pragma once
#include <QString>

namespace Rina::UI {
    class TimelineService;
    class ProjectService;
}

namespace Rina::Core {
    class ProjectSerializer {
    public:
        static bool save(const QString& path, const UI::TimelineService* timeline, const UI::ProjectService* project);
        static bool load(const QString& path, UI::TimelineService* timeline, UI::ProjectService* project);
    };
}