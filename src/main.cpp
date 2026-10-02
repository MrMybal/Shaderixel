#include "ui/App.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    sx::App app;
    return app.Run();
}
