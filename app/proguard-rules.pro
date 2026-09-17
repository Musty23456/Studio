# Almus Studio ProGuard rules.
# Keep native method signatures (JNI) intact.
-keepclasseswithmembernames class * {
    native <methods>;
}
-keep class com.almus.studio.model.** { *; }
-keep class com.almus.studio.engine.** { *; }
