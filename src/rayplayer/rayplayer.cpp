
#include <cstdint>
#include <cstdlib>
extern "C" {
#include "audio.h"
#include "raylib.h"
#include <assert.h>
#include <pthread.h>
#include <rlgl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
}

#include "rayplayer.hpp"
#include <fstream>
#include <string>

typedef struct AVList {
  AVPacket self;
  struct AVList *next;
} AVList;

typedef struct {
  AVList *head;
  AVList *last;
  int size;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  AVCodecContext *codecCtx;
} PQueue;

PQueue pq;

void init_pq() {
  pq.head = nullptr;
  pq.last = nullptr;
  pq.size = 0;
  pq.codecCtx = nullptr;
  pthread_mutex_init(&pq.mutex, nullptr);
  pthread_cond_init(&pq.cond, nullptr);
}

bool pq_empty() { return pq.size == 0; }

void pq_put(AVPacket packet) {
  pthread_mutex_lock(&pq.mutex);
  AVList *node = (AVList *)malloc(sizeof(AVList));
  node->self = packet;
  node->next = nullptr;
  if (pq_empty()) {
    pq.head = node;
    pq.last = node;
  } else {
    pq.last->next = node;
    pq.last = node;
  }
  pq.size++;
  pthread_cond_signal(&pq.cond);
  pthread_mutex_unlock(&pq.mutex);
}

AVPacket pq_get() {
  pthread_mutex_lock(&pq.mutex);
  while (pq_empty()) {
    pthread_cond_wait(&pq.cond, &pq.mutex);
  }
  AVList *node = pq.head;
  pq.head = pq.head->next;
  AVPacket p = node->self;
  free(node);
  if (pq.head == nullptr) {
    pq.last = nullptr;
  }
  pq.size--;
  pthread_mutex_unlock(&pq.mutex);
  return p;
}

void pq_free() {
  pthread_mutex_destroy(&pq.mutex);
  pthread_cond_destroy(&pq.cond);
  AVList *node = pq.head;
  while (node != nullptr) {
    AVList *next = node->next;
    av_packet_unref(&node->self);
    free(node);
    node = next;
  }
}

int audio_decode_frame(uint8_t *buf) {
  AVPacket packet = pq_get();
  int ret = avcodec_send_packet(pq.codecCtx, &packet);
  if (ret < 0) {
    av_packet_unref(&packet);
    return -1;
  }
  AVFrame *frame = av_frame_alloc();

  ret = avcodec_receive_frame(pq.codecCtx, frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    av_frame_free(&frame);
    av_packet_unref(&packet);
    return -1;
  } else if (ret < 0) {
    av_frame_free(&frame);
    av_packet_unref(&packet);
    return -1;
  }
  // Got frame
  uint f_size = frame->linesize[0];
  uint b_ch = f_size / pq.codecCtx->ch_layout.nb_channels;
  int res =
      audio_resampling(pq.codecCtx, frame, AV_SAMPLE_FMT_FLT, 2, 44100, buf);

  av_frame_free(&frame);
  av_packet_unref(&packet);
  return res;
}

static bool go_ahead = false;

inline bool file_exists(const std::string &name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

Player::Player() {
  InitAudioDevice();
  init_pq();

  pFormatCtx = avformat_alloc_context();
  img_convert_ctx = sws_alloc_context();
  sws_ctx = nullptr;
  assert(file_exists("../video.mp4") == true);
  avformat_open_input(&pFormatCtx, "../video.mp4", nullptr, nullptr);
  TraceLog(LOG_INFO, "CODEC: Format %s", pFormatCtx->iformat->long_name);
  avformat_find_stream_info(pFormatCtx, nullptr);
  videoStream = nullptr;
  audioStream = nullptr;
  videoPar = nullptr;
  audioPar = nullptr;
  pRGBFrame = nullptr;
  for (int i = 0; i < pFormatCtx->nb_streams; i++) {
    AVStream *tmpStream = pFormatCtx->streams[i];
    AVCodecParameters *tmpPar = tmpStream->codecpar;
    if (tmpPar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audioStream = tmpStream;
      audioPar = tmpPar;
      TraceLog(LOG_INFO, "CODEC: Audio sample rate %d, channels: %d",
               audioPar->sample_rate, audioPar->ch_layout.nb_channels);
      continue;
    }
    if (tmpPar->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStream = tmpStream;
      videoPar = tmpPar;
      TraceLog(LOG_INFO, "CODEC: Resolution %d x %d, type: %d", videoPar->width,
               videoPar->height, videoPar->codec_id);
      continue;
    }
  }
  if (!videoStream) {
    TraceLog(LOG_ERROR, "Could not find video stream.\n");
    exit(-1);
  }

  // return with error in case no audio stream was found
  if (!audioStream) {
    TraceLog(LOG_WARNING, "Could not find audio stream. Disabling audio.\n");
  }
  const AVCodec *videoCodec = avcodec_find_decoder(videoPar->codec_id);
  const AVCodec *audioCodec;

  audioCodec = avcodec_find_decoder(audioPar->codec_id);

  TraceLog(LOG_INFO, "CODEC: %s ID %d, Bit rate %ld", videoCodec->name,
           videoCodec->id, videoPar->bit_rate);
  TraceLog(LOG_INFO, "FPS: %d/%d, TBR: %d/%d, TimeBase: %d/%d",
           videoStream->avg_frame_rate.num, videoStream->avg_frame_rate.den,
           videoStream->r_frame_rate.num, videoStream->r_frame_rate.den,
           videoStream->time_base.num, videoStream->time_base.den);

  audioCodecCtx = avcodec_alloc_context3(audioCodec);
  videoCodecCtx = avcodec_alloc_context3(videoCodec);

  pq.codecCtx = audioCodecCtx;
  avcodec_parameters_to_context(videoCodecCtx, videoPar);

  avcodec_parameters_to_context(audioCodecCtx, audioPar);
  avcodec_open2(videoCodecCtx, videoCodec, nullptr);

  avcodec_open2(audioCodecCtx, audioCodec, nullptr);

  frame = av_frame_alloc();
  packet = av_packet_alloc();
  sws_ctx = sws_getContext(videoCodecCtx->width, videoCodecCtx->height,
                           videoCodecCtx->pix_fmt, videoCodecCtx->width,
                           videoCodecCtx->height, AV_PIX_FMT_RGB24,
                           SWS_FAST_BILINEAR, 0, 0, 0);

  targetFPS = videoStream->avg_frame_rate.num / videoStream->avg_frame_rate.den;

  SetTargetFPS(targetFPS);

  pRGBFrame = av_frame_alloc();
  pRGBFrame->format = AV_PIX_FMT_RGB24;
  pRGBFrame->width = videoCodecCtx->width;
  pRGBFrame->height = videoCodecCtx->height;
  av_frame_get_buffer(pRGBFrame, 0);
  vframe = 0;
}

Image Player::fetch_next_image() {
  Image img;
  img.height = videoCodecCtx->height;
  img.width = videoCodecCtx->width;
  img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
  img.mipmaps = 1;

  vframe++;
  while (av_read_frame(pFormatCtx, packet) >= 0) {
    if (packet->stream_index == videoStream->index) {
      // Getting frame from video
      int ret = avcodec_send_packet(videoCodecCtx, packet);
      av_packet_unref(packet);
      if (ret < 0) {
        // Error
        TraceLog(LOG_ERROR, "Error sending packet\n");
        continue;
      }
      while (ret >= 0) {
        ret = avcodec_receive_frame(videoCodecCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }
        sws_scale(sws_ctx, (uint8_t const *const *)frame->data, frame->linesize,
                  0, frame->height, pRGBFrame->data, pRGBFrame->linesize);
        img.data = pRGBFrame->data[0];
      }
      break;
    } else if (packet->stream_index == audioStream->index) {
      AVPacket *cloned = av_packet_clone(packet);
      pq_put(*cloned);
    }
    av_packet_unref(packet);
  }

  return img;
}

std::vector<uint8_t> Player::fetch_next_audio() {
  uint8_t *buffer = (uint8_t *)malloc(2048);

  uint8_t *origin = (uint8_t *)buffer;
  uint8_t *dbuf = (uint8_t *)buffer;
  static uint8_t audio_buf[19200];
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;
  int len1 = -1;
  int audio_size = -1;
  int len = 1 * sizeof(float) * 2; // Stereo
  static int jj = 0;
  ++jj;
  while (len > 0) {
    if (audio_buf_index >= audio_buf_size) {
      audio_size = audio_decode_frame(audio_buf);
      if (audio_size < 0) {
        // output silence
        TraceLog(LOG_WARNING, "Skipped one frame.\n");
        continue;
      } else {
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    len1 = audio_buf_size - audio_buf_index;

    if (len1 > len) {
      len1 = len;
    }

    memcpy(dbuf, audio_buf + audio_buf_index, len1);

    len -= len1;
    dbuf += len1;
    audio_buf_index += len1;
  }
  go_ahead = true;

  return std::vector(buffer, buffer + 2048);
}

Player::~Player() {
  // UnloadAudioStream(rayAStream);

  CloseWindow();
  CloseAudioDevice();

  av_frame_free(&frame);
  av_frame_free(&pRGBFrame);
  av_packet_unref(packet);
  av_packet_free(&packet);
  avcodec_free_context(&videoCodecCtx);
  sws_freeContext(sws_ctx);
  avformat_close_input(&pFormatCtx);
  pq_free();
}