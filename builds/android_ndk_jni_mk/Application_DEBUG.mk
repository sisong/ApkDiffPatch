APP_PLATFORM := android-27
APP_STL := c++_shared
APP_CFLAGS := -fsanitize=address -fno-omit-frame-pointer
APP_LDFLAGS := -fsanitize=address
APP_CPPFLAGS := -fexceptions
APP_BUILD_SCRIPT := Android_DEBUG.mk
APP_OPTIM        := debug
NDK_DEBUG := 1
APP_ABI := armeabi-v7a arm64-v8a x86 x86_64