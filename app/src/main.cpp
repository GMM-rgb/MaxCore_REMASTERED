#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <commctrl.h>
#elif defined(__APPLE__)
    #include <Metal/Metal.h>
#else

#endif

struct EditorElement { int id; };

namespace EditorApplication {
    inline void startup_application() {
        // HWND WindowHandle;

        // WindowHandle = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, );
        // DefWindowProc(WindowHandle);
    }
}

int main() {
    return 1;
}
