#include <iostream>
#include <alsa/asoundlib.h>

int main() {
    std::cout << "--- ALSA Diagnose Start ---" << std::endl;

    // 1. Compile-Time Check (Was stand im Header beim Kompilieren?)
    std::cout << "[Header]  Version: " << SND_LIB_VERSION_STR << std::endl;

    // 2. Runtime Check (Welche .so Bibliothek wurde wirklich geladen?)
    // Wenn hier eine andere Version steht als oben, hast du ein Linker-Problem!
    std::cout << "[Lib]     Version: " << snd_asoundlib_version() << std::endl;

    // 3. Pfad Check (Wo sucht ALSA nach seiner Konfiguration?)
    // Das ist der Beweis, dass er deinen Userspace nutzt.
    const char* config_dir = snd_config_topdir();
    if (config_dir) {
        std::cout << "[Config]  Pfad:    " << config_dir << std::endl;
    } else {
        std::cout << "[Config]  Pfad konnte nicht ermittelt werden." << std::endl;
    }

    std::cout << "--- ALSA Diagnose Ende ---" << std::endl;
    return 0;
}