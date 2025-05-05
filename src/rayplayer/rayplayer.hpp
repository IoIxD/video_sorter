#include <raylib.h>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
class Player {
private:
  int vframe;
  AVFormatContext *pFormatCtx;
  struct SwsContext *img_convert_ctx;
  struct SwsContext *sws_ctx;
  AVStream *videoStream;
  AVStream *audioStream;
  AVCodecParameters *videoPar;
  AVCodecParameters *audioPar;
  AVFrame *pRGBFrame;
  AVFrame *frame;
  AVPacket *packet;
  AVCodecContext *audioCodecCtx;
  AVCodecContext *videoCodecCtx;
  AudioStream rayAStream;
  int targetFPS;

  Shader shader;

public:
  Player();
  Image fetch_next_image();
  std::vector<uint8_t> fetch_next_audio();

  size_t frame_num() { return videoStream->nb_frames; }
  ~Player();
};