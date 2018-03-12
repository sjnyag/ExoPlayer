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
package com.google.android.exoplayer2.ext.flac;

import android.util.Log;

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.extractor.ExtractorInput;
import com.google.android.exoplayer2.util.FlacStreamInfo;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

/**
 * JNI wrapper for the libflac Flac decoder.
 */
/* package */ final class FlacDecoderJni {

  private static final int TEMP_BUFFER_SIZE = 8192; // The same buffer size which libflac has
  private static final int SEEK_BUFFER_SIZE = 1024 * 1024; // The same buffer size which libflac has

  private final long nativeDecoderContext;

  private ByteBuffer byteBufferData;
  private ExtractorInput extractorInput;
  private boolean endOfExtractorInput;
  private byte[] tempBuffer;
  private byte[] seekBuffer;
  private long currentPos = 0;
  private int seekSize = 0;
  private boolean isSeeking = false;

  public FlacDecoderJni() throws FlacDecoderException {
    if (!FlacLibrary.isAvailable()) {
      throw new FlacDecoderException("Failed to load decoder native libraries.");
    }
    nativeDecoderContext = flacInit();
    if (nativeDecoderContext == 0) {
      throw new FlacDecoderException("Failed to initialize decoder");
    }
  }

  /**
   * Sets data to be parsed by libflac.
   * @param byteBufferData Source {@link ByteBuffer}
   */
  public void setData(ByteBuffer byteBufferData) {
    this.byteBufferData = byteBufferData;
    this.extractorInput = null;
    this.tempBuffer = null;
  }

  /**
   * Sets data to be parsed by libflac.
   * @param extractorInput Source {@link ExtractorInput}
   */
  public void setData(ExtractorInput extractorInput) {
    this.byteBufferData = null;
    this.extractorInput = extractorInput;
    if (tempBuffer == null) {
      this.tempBuffer = new byte[TEMP_BUFFER_SIZE];
    }
    endOfExtractorInput = false;
  }

  public void setDataAfterSkip(ExtractorInput extractorInput) throws IOException, InterruptedException {
    isSeeking = false;
    seekBuffer = null;
    extractorInput.skipFully((int) currentPos);
  }

  public boolean isEndOfData() {
    if (byteBufferData != null) {
      return byteBufferData.remaining() == 0;
    } else if (extractorInput != null) {
      return endOfExtractorInput;
    }
    return true;
  }

  public boolean isSeeking(){
    return isSeeking;
  }

  public long getStreamLength(){
    if (extractorInput != null) {
      return extractorInput.getLength();
    } else {
      return -1;
    }
  }

  /**
   * Reads up to {@code length} bytes from the data source.
   * <p>
   * This method blocks until at least one byte of data can be read, the end of the input is
   * detected or an exception is thrown.
   * <p>
   * This method is called from the native code.
   *
   * @param target A target {@link ByteBuffer} into which data should be written.
   * @return Returns the number of bytes read, or -1 on failure. It's not an error if this returns
   * zero; it just means all the data read from the source.
   */
  public int read(ByteBuffer target, int offset) throws IOException, InterruptedException {
    int byteCount = target.remaining();
    if (byteBufferData != null) {
      byteCount = Math.min(byteCount, byteBufferData.remaining());
      int originalLimit = byteBufferData.limit();
      byteBufferData.limit(byteBufferData.position() + byteCount);

      target.put(byteBufferData);

      byteBufferData.limit(originalLimit);
    } else if (extractorInput != null) {
      int skip = offset - ((int) extractorInput.getPosition());
      Log.e("FlacDecoderJni", "byteCount: " + byteCount);
      Log.e("FlacDecoderJni", "offset: " + offset);
      Log.e("FlacDecoderJni", "getPosition: " + (int) extractorInput.getPosition());
      Log.e("FlacDecoderJni", "skip: " + skip);
      if(skip < 0){
        if(SEEK_BUFFER_SIZE + skip < 0){
          return -1;
        }
        byte[] buffered = Arrays.copyOfRange(seekBuffer,seekBuffer.length + skip, seekBuffer.length + skip + Math.min(skip * -1, TEMP_BUFFER_SIZE));
        byteCount = readFromExtractorInput(0, Math.min(byteCount - buffered.length, TEMP_BUFFER_SIZE));
        byte[] result = new byte[buffered.length + byteCount];
        System.arraycopy(buffered,0, result,0, buffered.length);
        System.arraycopy(tempBuffer,0, result, buffered.length, byteCount);
        target.put(result, 0, buffered.length + byteCount);
        return result.length;
      }
      while(true){
        skip -= readFromExtractorInput(0, Math.min(skip, TEMP_BUFFER_SIZE));
        if(skip <= 0 || skip == C.RESULT_END_OF_INPUT){
          break;
        }
      }
      byteCount = Math.min(byteCount, TEMP_BUFFER_SIZE);
      int read = readFromExtractorInput(0, byteCount);
      if (read < 4) {
        // Reading less than 4 bytes, most of the time, happens because of getting the bytes left in
        // the buffer of the input. Do another read to reduce the number of calls to this method
        // from the native code.
        read += readFromExtractorInput(read, byteCount - read);
      }
      byteCount = read;
      target.put(tempBuffer, 0, byteCount);
    } else {
      return -1;
    }
    return byteCount;
  }

  public FlacStreamInfo decodeMetadata() throws IOException, InterruptedException {
    return flacDecodeMetadata(nativeDecoderContext);
  }

  public int decodeSample(ByteBuffer output) throws IOException, InterruptedException {
    return output.isDirect()
        ? flacDecodeToBuffer(nativeDecoderContext, output)
        : flacDecodeToArray(nativeDecoderContext, output.array());
  }

  /**
   * Returns the position of the next data to be decoded, or -1 in case of error.
   */
  public long getDecodePosition() {
    return flacGetDecodePosition(nativeDecoderContext);
  }

  public long getLastSampleTimestamp() {
    return flacGetLastTimestamp(nativeDecoderContext);
  }

  /**
   * Maps a seek position in microseconds to a corresponding position (byte offset) in the flac
   * stream.
   *
   * @param timeUs A seek position in microseconds.
   * @return The corresponding position (byte offset) in the flac stream or -1 if the stream doesn't
   * have a seek table.
   */
  public long getSeekPosition(long timeUs) {
    return flacGetSeekPosition(nativeDecoderContext, timeUs);
  }

  public String getStateString() {
    return flacGetStateString(nativeDecoderContext);
  }

  public void flush() {
    flacFlush(nativeDecoderContext);
  }

  /**
   * Resets internal state of the decoder and sets the stream position.
   *
   * @param newPosition Stream's new position.
   */
  public void reset(long newPosition) {
    flacReset(nativeDecoderContext, newPosition);
  }

  public void seekAbsolute(long timeUs) {
    isSeeking = true;
    seekSize = 0;
    seekBuffer = new byte[SEEK_BUFFER_SIZE];
    flacSeekAbsolute(nativeDecoderContext, timeUs);
  }

  public void release() {
    flacRelease(nativeDecoderContext);
  }

  private int readFromExtractorInput(int offset, int length)
      throws IOException, InterruptedException {
    int read = extractorInput.read(tempBuffer, offset, length);
    if (read == C.RESULT_END_OF_INPUT) {
      endOfExtractorInput = true;
      read = 0;
    }
    if(isSeeking){
      byte[] tempSeekBuffer = new byte[SEEK_BUFFER_SIZE];
      System.arraycopy(seekBuffer, length, tempSeekBuffer, 0, seekBuffer.length - length);
      System.arraycopy(tempBuffer,0, tempSeekBuffer, seekBuffer.length - length, length);
      seekBuffer = tempSeekBuffer;
      seekSize += length;
    }
    currentPos = extractorInput.getPosition();
    return read;
  }

  private native long flacInit();
  private native FlacStreamInfo flacDecodeMetadata(long context)
      throws IOException, InterruptedException;
  private native int flacDecodeToBuffer(long context, ByteBuffer outputBuffer)
      throws IOException, InterruptedException;
  private native int flacDecodeToArray(long context, byte[] outputArray)
      throws IOException, InterruptedException;
  private native long flacGetDecodePosition(long context);
  private native long flacGetLastTimestamp(long context);
  private native long flacGetSeekPosition(long context, long timeUs);
  private native String flacGetStateString(long context);
  private native void flacFlush(long context);
  private native void flacReset(long context, long newPosition);
  private native void flacSeekAbsolute(long context, long timeUs);
  private native void flacRelease(long context);

}
