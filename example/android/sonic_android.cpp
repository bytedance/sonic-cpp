#include <jni.h>

#include <string>

#include "sonic/sonic.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_sonic_MainActivity_isValidJson(JNIEnv* env, jobject,
                                                jbyteArray json) {
  if (json == nullptr) {
    return JNI_FALSE;
  }

  const jsize size = env->GetArrayLength(json);
  std::string input(static_cast<size_t>(size), '\0');
  env->GetByteArrayRegion(json, 0, size,
                          reinterpret_cast<jbyte*>(input.data()));
  if (env->ExceptionCheck()) {
    return JNI_FALSE;
  }

  sonic_json::Document document;
  document.Parse(input);
  return document.HasParseError() ? JNI_FALSE : JNI_TRUE;
}
