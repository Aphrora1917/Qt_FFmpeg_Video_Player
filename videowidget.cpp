#include "videowidget.h"
#include "ui_videowidget.h"

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoWidget)
{
    ui->setupUi(this);

    QTimer::singleShot(1000, [=](){
        openVideo_single_thread("./test_video.mp4");
    });

    connect(this, &VideoWidget::updateImg, this, &VideoWidget::setImage);

    qDebug() << avcodec_version();
    qDebug() << avcodec_license();

    QTimer::singleShot(1000, [=](){
        m_pc = open_video("./test_video.mp4");
    });
    m_pc = std::make_shared<PlayerCore>();
}


int VideoWidget::openVideo_single_thread(const std::string &_url)
{
    AVFrame	*pFrame = nullptr, *rgb_frame = nullptr;
    struct SwsContext *sws_ctx = nullptr;
    int ret, got_picture;

    // 创建AVFormatContext来处理输入流
    AVFormatContext *fmt_ctx = nullptr;

    // 打开输入流（UDP单播）
    if (avformat_open_input(&fmt_ctx, _url.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "无法打开输入流: " << _url << std::endl;
            return -1;
    }

    // 获取输入流信息
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "无法获取输入流信息!" << std::endl;
        return -1;
    }

    // 查找视频流
    int video_stream_index = -1;
    AVCodecParameters *codec_params = nullptr;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            codec_params = fmt_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index == -1) {
        std::cerr << "未找到视频流!" << std::endl;
            return -1;
    }

    // 查找视频解码器
    const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        std::cerr << "无法找到解码器!" << std::endl;
            return -1;
    }

    // 创建解码器上下文
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "无法分配解码器上下文!" << std::endl;
        return -1;
    }

    // 配置解码器参数
    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        std::cerr << "无法将解码器参数传递到上下文!" << std::endl;
        return -1;
    }

    // 打开解码器
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "无法打开解码器!" << std::endl;
            return -1;
    }

    // 从输入流读取数据包并转发
    AVPacket packet;
    while (true) {
        // 从输入流读取数据包
        if (av_read_frame(fmt_ctx, &packet) < 0) {
            std::cerr << "无法从输入流读取数据!" << std::endl;
            break;
        }

        // 如果是视频流，渲染画面
        if (packet.stream_index == video_stream_index) {
            // 渲染代码
            // 将读到的视频包送入解码器
            int ret = avcodec_send_packet(codec_ctx, &packet);
            if (ret < 0) {
                std::cerr << "发送数据包到解码器失败: " << std::endl;
                av_packet_unref(&packet);
                continue; // 跳过该包
            }

            // 循环接收解码后的帧（一次 send 可能对应多个 receive）
            while (ret >= 0) {
                pFrame = av_frame_alloc();   // 存放解码后的原始帧（YUV等）
                ret = avcodec_receive_frame(codec_ctx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_frame_free(&pFrame);
                    break;
                } else if (ret < 0) {
                    std::cerr << "接收解码帧失败: " << std::endl;
                        av_frame_free(&pFrame);
                    break;
                }

                // 此时 pFrame 中是一帧原始格式（如 YUV420P）
                // --- 开始渲染：转换为 RGB24 ---

                if (!sws_ctx) {
                    // 创建转换上下文：源格式 -> 目标格式（RGB24）
                    sws_ctx = sws_getContext(
                        pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format,
                        pFrame->width, pFrame->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                    if (!sws_ctx) {
                        std::cerr << "无法创建图像转换上下文" << std::endl;
                        av_frame_free(&pFrame);
                        break;
                    }
                    // 分配目标帧
                    rgb_frame = av_frame_alloc();
                    rgb_frame->format = AV_PIX_FMT_RGB24;
                    rgb_frame->width = pFrame->width;
                    rgb_frame->height = pFrame->height;
                    av_frame_get_buffer(rgb_frame, 1);
                }

                // 执行转换
                sws_scale(sws_ctx,
                          pFrame->data, pFrame->linesize,
                          0, pFrame->height,
                          rgb_frame->data, rgb_frame->linesize);

                std::cout << "pFrame size: " << pFrame->pkt_size << "| pFrame duration: " << pFrame->duration << std::endl;
                std::cout << "rgb_frame size: " << rgb_frame->pkt_size << "| rgb_frame duration: " << rgb_frame->duration << std::endl;

                // renderingFrame(rgb_frame);
                // write_yuv_to_file(pFrame);

                // 创建
                QImage img (pFrame->width, pFrame->height, QImage::Format_RGB888);

                uint8_t* dst[] = { img.bits() };
                int dstStride[4];
                // dstStride[0] = img.bytesPerLine();  // 获取 Qt 内部实际的 Stride
                // AV_PIX_FMT_RGB24 对应于 QImage::Format_RGB888
                av_image_fill_linesizes(dstStride, AV_PIX_FMT_RGB24, pFrame->width);
                // 转换
                sws_scale(sws_ctx, pFrame->data, (const int*)pFrame->linesize,
                          0, pFrame->height, dst, dstStride);

                // setImage(img);

                emit updateImg(img);

                // --- 渲染结束 ---

                av_frame_free(&pFrame);
            } // while 接收帧结束
        }

        av_packet_unref(&packet);
    }

    // 清理资源
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}

std::shared_ptr<PlayerCore> VideoWidget::open_video(const std::string &_url)
{
    auto pc = std::make_shared<PlayerCore>();

    // 对PlayerCore进行初始化
    pc->filename = std::move(_url);

    // 对PlayerCore的PackageQueue和FrameQueue进行初始化


    // 启动read_thread线程
    m_pc->future_read = std::make_unique<std::future<int>>(std::async(std::launch::async, &VideoWidget::read_thread_func, this, pc));
}

int VideoWidget::read_thread_func(std::shared_ptr<PlayerCore> _pc)
{
    std::mutex wait_mutex;      // read_thread的锁，用于在PacketQueue满和空时控制read_thread的读取进度


    // 解协议操作，将视频协议信息传递给PlayerCore

    // 创建AVFormatContext来处理输入流
    AVFormatContext *fmt_ctx = nullptr;

    int ret = 0;

    // 打开输入流（UDP单播）
    if ((ret = avformat_open_input(&fmt_ctx, _pc->filename.c_str(), nullptr, nullptr)) < 0) {
        std::cerr << "无法打开输入流: " << _pc->filename << std::endl;
        output_ffmeg_error(ret);
        return ret;
    }

    // 获取输入流信息
    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0) {
        std::cerr << "无法获取输入流信息!" << std::endl;
        output_ffmeg_error(ret);
        return ret;
    }

    _pc->fmt_ctx = fmt_ctx;

    // 查找视频流、音频流、字幕流
    int stream_index[AVMEDIA_TYPE_NB];
    memset(stream_index, -1, sizeof(stream_index));

    // 为了稳妥和兼容性，调用ffmpeg提供的av_find_best_stream，而非直接轮询fmt_ctx->streams[i]->codecpar->codec_type
    stream_index[AVMEDIA_TYPE_VIDEO] =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO,
                            stream_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    stream_index[AVMEDIA_TYPE_AUDIO] =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO,
                            stream_index[AVMEDIA_TYPE_AUDIO],
                            stream_index[AVMEDIA_TYPE_VIDEO],
                            NULL, 0);

    stream_index[AVMEDIA_TYPE_SUBTITLE] =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_SUBTITLE,
                            stream_index[AVMEDIA_TYPE_SUBTITLE],
                            (stream_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 stream_index[AVMEDIA_TYPE_AUDIO] :
                                 stream_index[AVMEDIA_TYPE_VIDEO]),
                            NULL, 0);


    // 尝试打开流，依据打开情况，来选择要启动的解码线程
    int ret_video = 0;
    int ret_audio = 0;
    int ret_subtitle = 0;
    if (stream_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        // 尝试打开视频流相关组件
        ret_video = stream_component_open(_pc, stream_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (stream_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        // 尝试打开音频流相关组件
        ret_audio = stream_component_open(_pc, stream_index[AVMEDIA_TYPE_AUDIO]);
    }
    if (stream_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        // 尝试打开字幕流相关组件
        ret_subtitle = stream_component_open(_pc, stream_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    // 依据流组件打开结果设置PlayerCore的show_mode
    if (ret_video >= 0)
        _pc->show_mode = PlayerCore::ShowMode::SHOW_MODE_VIDEO;
    else if (ret_audio >=0)
        _pc->show_mode = PlayerCore::ShowMode::SHOW_MODE_AUDIO;
    else
        _pc->show_mode = PlayerCore::ShowMode::SHOW_MODE_NONE;


    // 解封装循环（read_thread）：不仅是读取，更是“调度中心”
    // 1.解封装获取AVPacket，并且对AVPacket按照流类型进行分发
    // 2.处理暂停（Pause）—— 让出 CPU
    // 3.处理跳转（Seek）—— 清空流水线
    // 4.流量控制（背压）—— 防止内存爆炸，PacketQueue空时read_thread还需被条件变量唤醒，获取AVPacket并将其入队
    // 5.处理结束（EOF）与循环（Loop）
    for(;;)
    {

    }
}

// 若返回 0，代表函数执行成功，其他值为 ffmpeg 错误码
int VideoWidget::stream_component_open(std::shared_ptr<PlayerCore> _pc, int stream_index)
{
    // 查找视频解码器、创建解码器上下文、配置解码器参数、打开解码器
    AVFormatContext *ic = _pc->fmt_ctx;
    AVCodecContext *avctx;
    const AVCodec *codec;
    AVDictionary *opts = NULL;
    int sample_rate;
    // AVChannelLayout ch_layout = { 0 };
    int ret = 0;
    int stream_lowres = 0;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    // 初始化空的解码器
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    // 分配解码器参数 codecpar 到解码器上下文 avctx
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
    {
        avcodec_free_context(&avctx);
        return ret;
    }

    // 设置包的时间基准（pkt_timebase），用于将 AVPacket.pts/dts 转换为解码器内部时间。
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    // 查找/获取视频解码器，根据解码器上下文的 codec_id 查找系统自带的解码器。
    codec = avcodec_find_decoder(avctx->codec_id);

    switch(avctx->codec_type){
    case AVMEDIA_TYPE_AUDIO   : _pc->last_audio_stream    = stream_index; break;
    case AVMEDIA_TYPE_SUBTITLE: _pc->last_subtitle_stream = stream_index; break;
    case AVMEDIA_TYPE_VIDEO   : _pc->last_video_stream    = stream_index; break;
    }

    // 判断解码器设置是否成功
    if (!codec) {
        ret = AVERROR(EINVAL);
        avcodec_free_context(&avctx);
        return ret;
    }
    avctx->codec_id = codec->id;

    // 降低解码质量保证在低性能设备上的播放，先注释忽略
    // if (stream_lowres > codec->max_lowres) {
    //     av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
    //            codec->max_lowres);
    //     stream_lowres = codec->max_lowres;
    // }
    // avctx->lowres = stream_lowres;

    // 若未指定 threads，则默认设为 "auto"（自动选择线程数）。
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);

    // 设置 lowres 到选项字典（覆盖之前直接赋值）。先注释忽略
    // if (stream_lowres)
    //     av_dict_set_int(&opts, "lowres", stream_lowres, 0);

    // 添加 flags=+copy_opaque，确保解码器会复制 AVPacket.opaque（用于传递帧位置信息）。先注释
    // av_dict_set(&opts, "flags", "+copy_opaque", AV_DICT_MULTIKEY);

    // 硬解，先注释，先用软解跑通，再打开注释进行硬件加速优化
    // if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    //     ret = create_hwaccel(&avctx->hw_device_ctx);
    //     if (ret < 0)
    //         goto fail;
    // }

    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        avcodec_free_context(&avctx);
        return ret;
    }

    _pc->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;

    // 解码器已配置完成，接下来是各解码线程的初始化操作和启动解码线程
    // 将解码器上下文、队列、状态变量和线程执行体绑定成一个完整的“解码会话（Decoding Session）”
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
    {
        _pc->video_stream = stream_index;
        _pc->video_st = ic->streams[stream_index];

        // 解码线程初始化
        decoder_init(&_pc->viddec, avctx, &_pc->audio_p_que, _pc->continue_read_thread);

        // 启动解码线程
        auto func = std::bind(&VideoWidget::video_thread_func, this, std::placeholders::_1);
        decoder_start(&_pc->viddec, func, _pc);

        break;
    }
    case AVMEDIA_TYPE_AUDIO:
    {
        // 需要打开音频设备


        _pc->audio_stream = stream_index;
        _pc->audio_st = ic->streams[stream_index];

        // 解码线程初始化
        decoder_init(&_pc->auddec, avctx, &_pc->audio_p_que, _pc->continue_read_thread);

        // 启动解码线程
        auto func = std::bind(&VideoWidget::audio_thread_func, this, std::placeholders::_1);
        decoder_start(&_pc->auddec, func, _pc);

        break;
    }
    case AVMEDIA_TYPE_SUBTITLE:
    {
        _pc->subtitle_stream = stream_index;
        _pc->subtitle_st = ic->streams[stream_index];

        // 解码线程初始化
        decoder_init(&_pc->subdec, avctx, &_pc->subtitle_p_que, _pc->continue_read_thread);

        // 启动解码线程
        auto func = std::bind(&VideoWidget::subtitle_thread_func, this, std::placeholders::_1);
        decoder_start(&_pc->subdec, func, _pc);

        break;
    }
    default:
        break;
    }

    return ret;
}

int VideoWidget::decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, std::shared_ptr<std::condition_variable> empty_queue_cond)
{
    d->pkt = av_packet_alloc();
    if (!d->pkt)
        return AVERROR(ENOMEM);
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
    d->pkt_serial = -1;

    d->queue->getLock();
    d->queue->abort_request = 0;
    d->queue->serial++;

    return 0;
}

int VideoWidget::decoder_start(Decoder *d, std::function<int (std::shared_ptr<PlayerCore>)> fn, std::shared_ptr<PlayerCore> _pc)
{
    d->decoder_thread = std::thread(fn, _pc);

    return 0;
}

void VideoWidget::decoder_abort(Decoder *d, FrameQueue *fq)
{

}

int VideoWidget::video_thread_func(std::shared_ptr<PlayerCore> _pc)
{

}

int VideoWidget::audio_thread_func(std::shared_ptr<PlayerCore> _pc)
{

}

int VideoWidget::subtitle_thread_func(std::shared_ptr<PlayerCore> _pc)
{

}

void VideoWidget::renderingFrame(const AVFrame *_frame)
{
    QImage image(_frame->data[0], _frame->width, _frame->height, _frame->linesize[0], QImage::Format_RGB32);
    QPixmap pixmap = QPixmap::fromImage(image);   // 拷贝！
    // ui->label->setPixmap(pixmap);

}

void VideoWidget::setImage(const QImage &image)
{
    currentImage = image;
    update();
    // repaint();
}


void VideoWidget::write_yuv_to_file(AVFrame *frame)
{
    FILE* f = fopen("debug_frame.yuv", "ab"); // 追加模式
    if (!f) return;
    // 写入Y平面数据 (逐行写入，以防 linesize 有额外填充)
    for (int i = 0; i < frame->height; i++) {
        fwrite(frame->data[0] + i * frame->linesize[0], 1, frame->width, f);
    }
    // 写入U平面数据
    for (int i = 0; i < frame->height / 2; i++) {
        fwrite(frame->data[1] + i * frame->linesize[1], 1, frame->width / 2, f);
    }
    // 写入V平面数据
    for (int i = 0; i < frame->height / 2; i++) {
        fwrite(frame->data[2] + i * frame->linesize[2], 1, frame->width / 2, f);
    }
    fclose(f);
}


void VideoWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if (!currentImage.isNull()) {
        painter.drawImage(QRect(0, 0, width(), height()), currentImage);
        // qDebug() << "w: " << currentImage.width() << "| h: " << currentImage.height();
    }
    // qDebug() << "w: " << width() << "| h: " << height();
}

void VideoWidget::mouseEvent(QMouseEvent *event)
{
    // 这里必须使用buttons()
    if(event->buttons() & Qt::LeftButton){  //进行的按位与
        // update();
        repaint();
        qDebug() << "mouse left btn clicked.";
    }
}

VideoWidget::~VideoWidget()
{
    delete ui;
}


