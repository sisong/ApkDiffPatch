APP_PLATFORM := android-27
APP_CFLAGS := -fsanitize=address -fno-omit-frame-pointer
APP_CPPFLAGS := -fexceptions
APP_LDFLAGS := -fsanitize=address
APP_LDFLAGS += -Wl,-z,max-page-size=16384
APP_STL := c++_shared
APP_BUILD_SCRIPT := Android_DEBUG.mk
APP_OPTIM        := debug
NDK_DEBUG := 1
APP_ABI := armeabi-v7a arm64-v8a x86 x86_64