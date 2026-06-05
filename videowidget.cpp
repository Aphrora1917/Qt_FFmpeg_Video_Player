#include "videowidget.h"
#include "ui_videowidget.h"

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoWidget)
{
    ui->setupUi(this);

    QTimer::singleShot(1000, [=](){
        openVideo("./test_video.mp4");
    });

    connect(this, &VideoWidget::updateImg, this, &VideoWidget::setImage);

    qDebug() << avcodec_version();
    qDebug() << avcodec_license();
}


int VideoWidget::openVideo(const std::string &_url)
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


