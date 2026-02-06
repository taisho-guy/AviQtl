#include "video_encoder.hpp"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace Rina::Core {

VideoEncoder::VideoEncoder(QObject *parent) : QObject(parent) {}

VideoEncoder::~VideoEncoder() { close(); }

void VideoEncoder::cleanup() {
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_hwFrame) {
        av_frame_free(&m_hwFrame);
    }
    if (m_swFrame) {
        av_frame_free(&m_swFrame);
    }
    if (m_encCtx) {
        avcodec_free_context(&m_encCtx);
    }
    if (m_fmtCtx) {
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_fmtCtx->pb);
        }
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
    }
}

bool VideoEncoder::initHardware(const QString &codecName) {
    int err = 0;
    // VA-API デバイスの作成 (Linux/Wayland/AMD)
    // 通常は /dev/dri/renderD128 等を自動検出
    if ((err = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0)) < 0) {
        qWarning() << "Failed to create VAAPI device context:" << err;
        return false;
    }
    qDebug() << "VAAPI hardware device initialized successfully.";
    return true;
}

bool VideoEncoder::open(const Config &config) {
    cleanup();
    m_config = config;

    // 1. コンテナフォーマットの初期化
    avformat_alloc_output_context2(&m_fmtCtx, nullptr, nullptr, config.outputUrl.toStdString().c_str());
    if (!m_fmtCtx) {
        qWarning() << "Could not deduce output format from file extension.";
        return false;
    }

    // 2. コーデックの検索
    const AVCodec *codec = avcodec_find_encoder_by_name(config.codecName.toStdString().c_str());
    if (!codec) {
        qWarning() << "Codec not found:" << config.codecName;
        return false;
    }

    m_stream = avformat_new_stream(m_fmtCtx, codec);
    if (!m_stream)
        return false;

    m_encCtx = avcodec_alloc_context3(codec);
    if (!m_encCtx)
        return false;

    // 3. ハードウェア初期化 (VA-API)
    if (config.codecName.contains("vaapi")) {
        if (!initHardware(config.codecName))
            return false;

        // ハードウェアフレームコンテキストの設定
        AVBufferRef *hw_frames_ref = av_hwframe_ctx_alloc(m_hwDeviceCtx);
        AVHWFramesContext *frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
        frames_ctx->format = AV_PIX_FMT_VAAPI;
        frames_ctx->sw_format = AV_PIX_FMT_NV12; // VAAPIの一般的な入力フォーマット
        frames_ctx->width = config.width;
        frames_ctx->height = config.height;
        frames_ctx->initial_pool_size = 32; // プールサイズを増加 (20 -> 32)

        if (av_hwframe_ctx_init(hw_frames_ref) < 0) {
            qWarning() << "Failed to initialize VAAPI frame context.";
            av_buffer_unref(&hw_frames_ref);
            return false;
        }
        m_encCtx->hw_frames_ctx = hw_frames_ref;
    }

    // 4. エンコーダパラメータ設定
    m_encCtx->width = config.width;
    m_encCtx->height = config.height;
    m_encCtx->time_base = {config.fps_den, config.fps_num};
    m_stream->time_base = m_encCtx->time_base;
    m_encCtx->pix_fmt = AV_PIX_FMT_VAAPI; // HWエンコード用フォーマット

    // ビットレート制御 (CBR/VBR) - 簡易設定
    m_encCtx->bit_rate = config.bitrate;
    m_encCtx->rc_max_rate = config.bitrate;
    m_encCtx->rc_buffer_size = config.bitrate / 2; // 0.5秒バッファ

    // グローバルヘッダーが必要なコンテナ(mp4等)の場合
    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_encCtx, codec, nullptr) < 0) {
        qWarning() << "Could not open codec.";
        return false;
    }

    avcodec_parameters_from_context(m_stream->codecpar, m_encCtx);

    // 5. ファイルオープン
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmtCtx->pb, config.outputUrl.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
            qWarning() << "Could not open output file:" << config.outputUrl;
            return false;
        }
    }

    if (avformat_write_header(m_fmtCtx, nullptr) < 0) {
        qWarning() << "Error occurred when opening output file.";
        return false;
    }

    // 6. フレーム確保
    m_swFrame = av_frame_alloc();
    m_swFrame->format = AV_PIX_FMT_NV12; // SW変換用中間バッファ
    m_swFrame->width = config.width;
    m_swFrame->height = config.height;
    av_frame_get_buffer(m_swFrame, 32);

    m_hwFrame = av_frame_alloc(); // HW転送用

    qDebug() << "VideoEncoder opened using codec:" << config.codecName;
    return true;
}

bool VideoEncoder::open(const QVariantMap &configMap) {
    Config config;
    config.width = configMap.value("width").toInt();
    config.height = configMap.value("height").toInt();
    config.fps_num = configMap.value("fps_num").toInt();
    config.fps_den = configMap.value("fps_den").toInt();
    config.bitrate = configMap.value("bitrate").toInt();
    config.outputUrl = configMap.value("outputUrl").toString();
    // codecName defaults to h264_vaapi if not present
    return open(config);
}

bool VideoEncoder::pushFrame(const QImage &img, int64_t pts) {
    if (!m_encCtx)
        return false;

    // 1. QImage (ARGB32) -> SW Frame (NV12) 変換
    if (!m_swsCtx) {
        m_swsCtx = sws_getContext(img.width(), img.height(), AV_PIX_FMT_RGBA, // 【重要】入力はRGBA
                                  m_config.width, m_config.height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    // SWフレームを書き込み可能にする
    if (av_frame_make_writable(m_swFrame) < 0)
        return false;

    // QImageのメモリレイアウトに合わせる
    const uint8_t *srcData[1] = {img.bits()};
    int srcLinesize[1] = {static_cast<int>(img.bytesPerLine())};

    // 変換実行
    sws_scale(m_swsCtx, srcData, srcLinesize, 0, img.height(), m_swFrame->data, m_swFrame->linesize);
    m_swFrame->pts = pts;

    // 2. SW Frame -> HW Frame 転送 (CPU -> GPU Upload)
    // ここがボトルネックだが、QImageベースである以上回避不可。
    // 将来的にはここを dmabuf import に置き換える。
    if (av_hwframe_get_buffer(m_encCtx->hw_frames_ctx, m_hwFrame, 0) < 0) {
        qWarning() << "Failed to allocate HW frame.";
        return false;
    }
    if (av_hwframe_transfer_data(m_hwFrame, m_swFrame, 0) < 0) {
        qWarning() << "Failed to transfer data to GPU.";
        av_frame_unref(m_hwFrame); // 転送失敗時も参照を解放
        return false;
    }
    m_hwFrame->pts = pts;

    // 3. エンコード
    int ret = avcodec_send_frame(m_encCtx, m_hwFrame);

    // 【重要】送信したHWフレームはエンコーダー内部で参照されるため、
    // 呼び出し元（ここ）での参照を解放してプールに戻す必要がある。
    av_frame_unref(m_hwFrame);

    if (ret < 0) {
        qWarning() << "Error sending frame to codec.";
        return false;
    }

    while (ret >= 0) {
        AVPacket *pkt = av_packet_alloc();
        ret = avcodec_receive_packet(m_encCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            qWarning() << "Error during encoding.";
            av_packet_free(&pkt);
            return false;
        }

        av_packet_rescale_ts(pkt, m_encCtx->time_base, m_stream->time_base);
        pkt->stream_index = m_stream->index;

        av_interleaved_write_frame(m_fmtCtx, pkt);
        av_packet_free(&pkt); // パケット構造体とデータの解放
    }

    return true;
}

bool VideoEncoder::pushTexture(unsigned int textureId, const QSize &size, int64_t pts) {
    if (!m_encCtx)
        return false;

    // 現在のGLコンテキストを取得 (TimelineController側でmakeCurrentされている前提)
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning() << "No active OpenGL context for texture readback";
        return false;
    }
    // 【修正】ファクトリー経由で安全に取得
    auto *f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
    if (!f) {
        qWarning() << "Could not obtain OpenGL 3.3 Core functions. Context version:" << ctx->format().version();
        return false;
    }
    f->initializeOpenGLFunctions();

    // 1. SWフレームの準備 (NV12変換用バッファとしてRGBAを受け取る)
    if (!m_swsCtx) {
        m_swsCtx = sws_getContext(size.width(), size.height(), AV_PIX_FMT_RGBA, // OpenGL texture is RGBA
                                  m_config.width, m_config.height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
    }
    if (!m_swsCtx)
        return false;

    // 一時的なRGBAバッファ (初回のみ確保)
    static std::vector<uint8_t> rgbaBuffer;
    size_t neededSize = size.width() * size.height() * 4;
    if (rgbaBuffer.size() < neededSize)
        rgbaBuffer.resize(neededSize);

    // 2. GPU Texture -> CPU Buffer (Raw Read)
    f->glBindTexture(GL_TEXTURE_2D, textureId);
    f->glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaBuffer.data());
    f->glBindTexture(GL_TEXTURE_2D, 0);

    // 3. RGBA -> NV12 (SwsScale)
    if (av_frame_make_writable(m_swFrame) < 0)
        return false;

    const int in_linesize[1] = {static_cast<int>(size.width() * 4)};
    const uint8_t *in_data[1] = {rgbaBuffer.data()};

    sws_scale(m_swsCtx, in_data, in_linesize, 0, size.height(), m_swFrame->data, m_swFrame->linesize);
    m_swFrame->pts = pts;

    // 4. HW Upload (VAAPI)
    if (av_hwframe_get_buffer(m_encCtx->hw_frames_ctx, m_hwFrame, 0) < 0)
        return false;
    if (av_hwframe_transfer_data(m_hwFrame, m_swFrame, 0) < 0) {
        av_frame_unref(m_hwFrame);
        return false;
    }
    m_hwFrame->pts = pts;

    // 5. Encode
    int ret = avcodec_send_frame(m_encCtx, m_hwFrame);
    av_frame_unref(m_hwFrame);
    if (ret < 0)
        return false;

    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
        return false;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_encCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_packet_free(&pkt);
            return false;
        }
        av_packet_rescale_ts(pkt, m_encCtx->time_base, m_stream->time_base);
        pkt->stream_index = m_stream->index;
        av_interleaved_write_frame(m_fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return true;
}

void VideoEncoder::close() {
    if (!m_encCtx)
        return;

    // フラッシュ処理
    avcodec_send_frame(m_encCtx, nullptr);
    while (true) {
        AVPacket *pkt = av_packet_alloc();
        int ret = avcodec_receive_packet(m_encCtx, pkt);
        if (ret < 0) {
            av_packet_free(&pkt);
            break;
        }
        av_packet_rescale_ts(pkt, m_encCtx->time_base, m_stream->time_base);
        pkt->stream_index = m_stream->index;
        av_interleaved_write_frame(m_fmtCtx, pkt);
        av_packet_free(&pkt);
    }

    av_write_trailer(m_fmtCtx);
    cleanup();
    qDebug() << "VideoEncoder closed.";
}

} // namespace Rina::Core