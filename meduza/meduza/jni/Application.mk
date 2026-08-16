# Application.mk

APP_ABI := arm64-v8a x86_64

APP_PLATFORM := android-23                      # minimal version — choose according to your needs (≥21 recommended)

APP_STL := c++_static                           # or c++_shared / system — c++_static is usually better for small final size

APP_CPPFLAGS := \
    -std=c++17 \
    -fno-rtti \
    -fno-exceptions \
    -fvisibility=hidden \
    -fvisibility-inlines-hidden \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -Oz \
    

# Very aggressive size & stripping flags (be careful — sometimes breaks code)
APP_LDFLAGS := \
    -Wl,--gc-sections \
    -Wl,--strip-all \
    -Wl,--build-id=none \
    -Wl,--hash-style=gnu \
    -Wl,--no-undefined \
    -pie \
    -Wl,-z,relro \
    -Wl,-z,now \
    -Wl,-z,noexecstack

# If you want LTO also at link stage (often gives best size reduction)
