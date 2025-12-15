#include "../../src/ssl/audio_capture.h"
#include "../../../Spatial_Audio_Framework/framework/include/saf.h"
#include "../../../Spatial_Audio_Framework/examples/include/array2sh.h"



int main() {
    AudioCapture mic(MIC_CFG_ZYLIA_ZM_1);
    mic.initialize();
    mic.start();

    // SAF
    // TODO: https://github.com/leomccormack/Spatial_Audio_Framework/tree/master/examples/src/array2sh
    return 0;
}
