# TEN VAD Component for ESP-IDF
idf_component_register(
    SRCS 
        "../../../src/aed.cc"
        "../../../src/biquad.cc"
        "../../../src/fftw.c"
        "../../../src/fscvrt.cc"
        "../../../src/pitch_est.cc"
        "../../../src/stft.cc"
        "../../../src/ten_vad.cc"
    INCLUDE_DIRS 
        "../../../include"
        "../../../src"
    REQUIRES 
        freertos
)

# Add ESP32-specific compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
)
