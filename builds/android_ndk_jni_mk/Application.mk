APP_STL := c++_static
APP_CPPFLAGS := -frtti -std=c++11 -fexceptions
APP_CFLAGS += -Wno-error=format-security
APP_CFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
APP_CFLAGS += -ffunction-sections -fdata-sections
APP_LDFLAGS += -Wl,--gc-sections
APP_PLATFORM := android-14
APP_BUILD_SCRIPT := Android.mk
APP_ABI := armeabi-v7a arm64-v8a x86 x86_64