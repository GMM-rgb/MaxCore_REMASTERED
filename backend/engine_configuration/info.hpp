#ifndef MAXCORE_MEC_INFO

#define MAXCORE_MEC_INFO
#define ENGINE_NAME "MEC (MaxCore Engine Components)" // MaxCore Engine Components

#include <string>
#include <string_view>

namespace info {
    struct EngineNameType {};
    struct EngineVersionType {};
    struct FrontendNameType {};

    template<typename name> struct InfoRegistry;

#ifdef ENGINE_NAME
    template<> struct InfoRegistry<EngineNameType> { static constexpr std::string_view value = ENGINE_NAME; };
    template<> struct InfoRegistry<EngineVersionType> { static constexpr std::string_view value = "1.6.0-REMASTERED"; };
    template<> struct InfoRegistry<FrontendNameType> { static constexpr std::string_view value = "MaxCore"; };
#endif

    template<typename target> constexpr std::string_view get_info() {
        return InfoRegistry<target>::value;
    }

    inline std::string get_debug_name() {
        return "[" + std::string(get_info<EngineNameType>()) + " DEBUG] ";
    }

    inline std::string to_string(std::string_view value) {
        return std::string(value);
    }
}

#ifndef EngineFrontendString
#define EngineFrontendString info::get_info<info::FrontendNameType>();
#endif

#endif // !MAXCORE_MEC_INFO
