#pragma once
#include <memory>

namespace Rina::Core {
    class Context {
    public:
        static Context& instance() {
            static Context inst;
            return inst;
        }
        // Dependency Injection containers will go here
    private:
        Context() = default;
    };
}
