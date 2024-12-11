rcp build.rcp


set APK="%CD%\build\%AppName%.apk"
set SDK=C:\Users\User\AppData\Local\Android\Sdk
set NDK=%SDK%\ndk\21.3.6528147
set JDK=D:\_android\jdk-16.0.1
set BuildTools=%SDK%\build-tools\32.1.0-rc1
set AndroidJar="%SDK%\platforms\android-32\android.jar"

clang array.obj init.obj io.obj main.obj math.obj mem.obj os.obj str.obj

