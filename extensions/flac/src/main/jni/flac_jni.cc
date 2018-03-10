/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>
#include <android/log.h>
#include <cstdlib>
#include "include/flac_parser.h"

//static JavaVM *jvm=NULL;
//#include <pthread.h>
//static pthread_key_t jnienv_key;
//
//jobject g_obj;
//
//
//void _android_key_cleanup(void *data)
//{
//	JNIEnv* env=(JNIEnv*)pthread_getspecific(jnienv_key);
//	if (env != NULL)
//	{
//		(*jvm).DetachCurrentThread();
//		pthread_setspecific(jnienv_key,NULL);
//	}
//}
//void av_set_jvm(JavaVM *vm)
//{
//	jvm=vm;
//	pthread_key_create(&jnienv_key,_android_key_cleanup);
//
//}
//JavaVM *av_get_jvm(void)
//{
//	return jvm;
//}
//
//JNIEnv *av_get_jni_env(void)
//{
//	JNIEnv *env=NULL;
//	if (jvm==NULL)
//	{
//		//ms_fatal("Calling ms_get_jni_env() while no jvm has been set using ms_set_jvm().");
//	}
//	else
//	{
//		env=(JNIEnv*)pthread_getspecific(jnienv_key);
//		if (env==NULL)
//		{
//			if ((*jvm).AttachCurrentThread(&env,NULL)!=0)
//			{
//				//ms_fatal("AttachCurrentThread() failed !");
//				return NULL;
//			}
//			pthread_setspecific(jnienv_key,env);
//		}
//	}
//	return env;
//}
//
//JNIEXPORT jint JNICALL  JNI_OnLoad(JavaVM *ajvm, void *reserved)
//{
//	av_set_jvm(ajvm);
//	__android_log_print(ANDROID_LOG_INFO,"avplayer_jni","JNI_OnLoad");
//	return JNI_VERSION_1_2;
//}

#define LOG_TAG "flac_jni"
#define ALOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#define ALOGV(...) \
  ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))

#define DECODER_FUNC(RETURN_TYPE, NAME, ...)                               \
  extern "C" {                                                             \
  JNIEXPORT RETURN_TYPE                                                    \
      Java_com_google_android_exoplayer2_ext_flac_FlacDecoderJni_##NAME( \
          JNIEnv *env, jobject thiz, ##__VA_ARGS__);                       \
  }                                                                        \
  JNIEXPORT RETURN_TYPE                                                    \
      Java_com_google_android_exoplayer2_ext_flac_FlacDecoderJni_##NAME( \
          JNIEnv *env, jobject thiz, ##__VA_ARGS__)

class JavaDataSource : public DataSource {
 public:
  void setFlacDecoderJni(JNIEnv *env, jobject flacDecoderJni) {
    this->env = env;
    this->flacDecoderJni = flacDecoderJni;



    if (mid == NULL) {
      jclass cls = env->GetObjectClass(flacDecoderJni);
      mid = env->GetMethodID(cls, "read", "(Ljava/nio/ByteBuffer;)I");
      env->DeleteLocalRef(cls);
    }

  }

  ssize_t readAt(off64_t offset, void *const data, size_t size) {
    __android_log_print(ANDROID_LOG_ERROR, "readAt", "offset %zd", offset);
    jobject byteBuffer = env->NewDirectByteBuffer(data, size);
    int result = env->CallIntMethod(flacDecoderJni, mid, byteBuffer);
    if (env->ExceptionCheck()) {
       // Exception is thrown in Java when returning from the native call.
       result = -1;
     }
    env->DeleteLocalRef(byteBuffer);




//    JNIEnv *current=NULL;
//    current=av_get_jni_env();
//    if(g_obj == NULL){
//        g_obj = current->NewGlobalRef(this->flacDecoderJni);
//    }
//    jclass clsss = current->GetObjectClass(g_obj);
//    jmethodID readMethodId = current->GetMethodID(clsss, "read", "(Ljava/nio/ByteBuffer;I)I");
//    jmethodID getStreamLengthMethodId = current->GetMethodID(clsss, "getStreamLength", "()J");
//    current->DeleteLocalRef(clsss);
//    __android_log_print(ANDROID_LOG_ERROR, "readAt", "mCurrentPos %zd", offset);
//    jobject byteBuffer = current->NewDirectByteBuffer(data, size);
//    __android_log_print(ANDROID_LOG_ERROR, "readAt", "1");
//    streamLength = current->CallLongMethod(g_obj, getStreamLengthMethodId);
//    __android_log_print(ANDROID_LOG_ERROR, "readAt", "2");
//    int result = current->CallIntMethod(g_obj, readMethodId, byteBuffer, offset);
//    if (current->ExceptionCheck()) {
//      // Exception is thrown in Java when returning from the native call.
//      result = -1;
//    }
//    __android_log_print(ANDROID_LOG_ERROR, "readAt", "3");
//    current->DeleteLocalRef(byteBuffer);
    return result;
  }

  ssize_t getStreamLength() {
    __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "getStreamLength");
    return streamLength;
  }

 private:
  JNIEnv *env;
  jobject flacDecoderJni;
  jmethodID mid;
  ssize_t streamLength;
};

struct Context {
  JavaDataSource *source;
  FLACParser *parser;

  Context() {
    source = new JavaDataSource();
    parser = new FLACParser(source);
  }

  ~Context() {
    delete parser;
    delete source;
  }
};

DECODER_FUNC(jlong, flacInit) {
  Context *context = new Context;
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacInit");
  if (!context->parser->init()) {
    delete context;
    return 0;
  }
  return reinterpret_cast<intptr_t>(context);
}

DECODER_FUNC(jobject, flacDecodeMetadata, jlong jContext) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacDecodeMetadata");
  Context *context = reinterpret_cast<Context *>(jContext);
  context->source->setFlacDecoderJni(env, thiz);
  if (!context->parser->decodeMetadata()) {
    return NULL;
  }

  const FLAC__StreamMetadata_StreamInfo &streamInfo =
      context->parser->getStreamInfo();

  jclass cls = env->FindClass(
      "com/google/android/exoplayer2/util/"
      "FlacStreamInfo");
  jmethodID constructor = env->GetMethodID(cls, "<init>", "(IIIIIIIJ)V");

  return env->NewObject(cls, constructor, streamInfo.min_blocksize,
                        streamInfo.max_blocksize, streamInfo.min_framesize,
                        streamInfo.max_framesize, streamInfo.sample_rate,
                        streamInfo.channels, streamInfo.bits_per_sample,
                        streamInfo.total_samples);
}

DECODER_FUNC(jint, flacDecodeToBuffer, jlong jContext, jobject jOutputBuffer) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacDecodeToBuffer");
  Context *context = reinterpret_cast<Context *>(jContext);
  context->source->setFlacDecoderJni(env, thiz);
  void *outputBuffer = env->GetDirectBufferAddress(jOutputBuffer);
  jint outputSize = env->GetDirectBufferCapacity(jOutputBuffer);
  return context->parser->readBuffer(outputBuffer, outputSize);
}

DECODER_FUNC(jint, flacDecodeToArray, jlong jContext, jbyteArray jOutputArray) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacDecodeToArray");
  Context *context = reinterpret_cast<Context *>(jContext);
  context->source->setFlacDecoderJni(env, thiz);
  jbyte *outputBuffer = env->GetByteArrayElements(jOutputArray, NULL);
  jint outputSize = env->GetArrayLength(jOutputArray);
  int count = context->parser->readBuffer(outputBuffer, outputSize);
  env->ReleaseByteArrayElements(jOutputArray, outputBuffer, 0);
  return count;
}

DECODER_FUNC(jlong, flacGetDecodePosition, jlong jContext) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacGetDecodePosition");
  Context *context = reinterpret_cast<Context *>(jContext);
  return context->parser->getDecodePosition();
}

DECODER_FUNC(jlong, flacGetLastTimestamp, jlong jContext) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacGetLastTimestamp");
  Context *context = reinterpret_cast<Context *>(jContext);
  return context->parser->getLastTimestamp();
}

DECODER_FUNC(jlong, flacGetSeekPosition, jlong jContext, jlong timeUs) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacGetSeekPosition");
  Context *context = reinterpret_cast<Context *>(jContext);
  return context->parser->getSeekPosition(timeUs);
}

DECODER_FUNC(jstring, flacGetStateString, jlong jContext) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacGetStateString");
  Context *context = reinterpret_cast<Context *>(jContext);
  const char *str = context->parser->getDecoderStateString();
  return env->NewStringUTF(str);
}

DECODER_FUNC(void, flacFlush, jlong jContext) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacFlush");
  Context *context = reinterpret_cast<Context *>(jContext);
  context->parser->flush();
}

DECODER_FUNC(void, flacReset, jlong jContext, jlong newPosition) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacReset");
  Context *context = reinterpret_cast<Context *>(jContext);
  context->parser->reset(newPosition);
}

DECODER_FUNC(void, flacSeekAbsolute, jlong jContext, jlong timeUs) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacSeekAbsolute");
  Context *context = reinterpret_cast<Context *>(jContext);
  context->parser->seekAbsolute(timeUs);
}

DECODER_FUNC(void, flacRelease, jlong jContext) {
  __android_log_print(ANDROID_LOG_ERROR, "flac_jni", "flacRelease");
  Context *context = reinterpret_cast<Context *>(jContext);
  delete context;
}
