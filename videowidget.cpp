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
    m_pc->future_read = std::make_unique<std::future<int>>(std::async(
        std::launch::async, &VideoWidget::read_thread_func, this, pc)
    );
}

int VideoWidget::read_thread_func(std::shared_ptr<PlayerCore> _pc)
{
    // 解协议操作，将视频协议信息传递给PlayerCore，尝试启动解码线程，读取AVPacket存入各解码器的的PacketQueue

    AVFormatContext *fmt_ctx = nullptr;
    AVPacket *pkt = NULL;
    std::mutex wait_mutex;      // read_thread的锁，用于在PacketQueue满和空时控制read_thread的读取进度
    int ret = 0;
    int ret_video = 0;
    int ret_audio = 0;
    int ret_subtitle = 0;
    int genpts = 0;
    int st_index[AVMEDIA_TYPE_NB];
    memset(st_index, -1, sizeof(st_index));



    // 创建AVFormatContext来处理输入流
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate packet.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    fmt_ctx->interrupt_callback.callback = decode_interrupt_cb;
    fmt_ctx->interrupt_callback.opaque = _pc.get();

    // 打开输入流（UDP单播）
    if ((ret = avformat_open_input(&fmt_ctx, _pc->filename.c_str(), nullptr, nullptr)) < 0) {
        std::cerr << "无法打开输入流: " << _pc->filename << std::endl;
        output_ffmeg_error(ret);
        return ret;
    }

    // 如果文件中的 PTS 不完整，强制 FFmpeg 根据 DTS 生成 PTS（对某些格式很重要）。
    // 默认 genpts 为 0，视需求而定。如果处理的是 MP4/MKV 等标准封装，PTS 通常完整，可以不开启）。
    if (genpts)
        fmt_ctx->flags |= AVFMT_FLAG_GENPTS;

    // 获取输入流信息
    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0) {
        std::cerr << "无法获取输入流信息!" << std::endl;
        output_ffmeg_error(ret);
        return ret;
    }

    _pc->fmt_ctx = fmt_ctx;

    if (fmt_ctx->pb)
        fmt_ctx->pb->eof_reached = 0;

    _pc->max_frame_duration = (fmt_ctx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    _pc->realtime = is_realtime(fmt_ctx);

    // 查找视频流、音频流、字幕流


    // 为了稳妥和兼容性，调用ffmpeg提供的av_find_best_stream，而非直接轮询fmt_ctx->streams[i]->codecpar->codec_type
    st_index[AVMEDIA_TYPE_VIDEO] =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO,
                            st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    st_index[AVMEDIA_TYPE_AUDIO] =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO,
                            st_index[AVMEDIA_TYPE_AUDIO],
                            st_index[AVMEDIA_TYPE_VIDEO],
                            NULL, 0);

    st_index[AVMEDIA_TYPE_SUBTITLE] =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_SUBTITLE,
                            st_index[AVMEDIA_TYPE_SUBTITLE],
                            (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 st_index[AVMEDIA_TYPE_AUDIO] :
                                 st_index[AVMEDIA_TYPE_VIDEO]),
                            NULL, 0);


    // 尝试打开流，依据打开情况，来选择要启动的解码线程

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        // 尝试打开视频流相关组件
        ret_video = stream_component_open(_pc, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        // 尝试打开音频流相关组件
        ret_audio = stream_component_open(_pc, st_index[AVMEDIA_TYPE_AUDIO]);
    }
    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        // 尝试打开字幕流相关组件
        ret_subtitle = stream_component_open(_pc, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    // 依据流组件打开结果设置PlayerCore的show_mode
    if (ret_video >= 0)
        _pc->show_mode = PlayerCore::ShowMode::SHOW_MODE_VIDEO;
    else if (ret_audio >=0)
        _pc->show_mode = PlayerCore::ShowMode::SHOW_MODE_AUDIO;
    else
        _pc->show_mode = PlayerCore::ShowMode::SHOW_MODE_NONE;

    if (_pc->video_stream < 0 && _pc->audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file ...");
        ret = -1;
        goto fail;
    }


    // 如果打开文件时需要从中间位置开始播放（比如记住上次播放进度），可以保留。否则删掉。
    // /* if seeking requested, we execute it */
    // if (_pc->start_time != AV_NOPTS_VALUE) {
    //     int64_t timestamp;

    //     timestamp = _pc->start_time;
    //     /* add the stream start time */
    //     if (fmt_ctx->start_time != AV_NOPTS_VALUE)
    //         timestamp += fmt_ctx->start_time;
    //     ret = avformat_seek_file(fmt_ctx, -1, INT64_MIN, timestamp, INT64_MAX, 0);
    //     if (ret < 0) {
    //         av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
    //                _pc->filename.c_str(), (double)timestamp / AV_TIME_BASE);
    //     }
    // }


    // 解封装循环（read_thread）：不仅是读取，更是“调度中心”
    // 1.解封装获取AVPacket，并且对AVPacket按照流类型进行分发
    // 2.处理暂停（Pause）—— 让出 CPU
    // 3.处理跳转（Seek）—— 清空流水线
    // 4.流量控制（背压）—— 防止内存爆炸，PacketQueue空时read_thread还需被条件变量唤醒，获取AVPacket并将其入队
    // 5.处理结束（EOF）与循环（Loop）
    for (;;) {
        // 检查是否终止循环
        if (_pc->abort_request)
            break;

        // 处理暂停 / 恢复状态变化
        if (_pc->paused != _pc->last_paused) {
            _pc->last_paused = _pc->paused;
            if (_pc->paused)
                _pc->read_pause_return = av_read_pause(fmt_ctx);
            else
                av_read_play(fmt_ctx);
        }

        // 处理 Seek（跳转）请求
        if (_pc->seek_req) {
            int64_t seek_target = _pc->seek_pos;     // 目标位置。
            // 计算 Seek 的边界范围。+2 和 -2 是为了修正边界舍入误差。
            int64_t seek_min    = _pc->seek_rel > 0 ? seek_target - _pc->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = _pc->seek_rel < 0 ? seek_target - _pc->seek_rel - 2: INT64_MAX;
            // FIXME the +-2 is due to rounding being not done in the correct direction in generation
            //      of the seek_pos/seek_rel variables

            // 执行 seek
            ret = avformat_seek_file(_pc->fmt_ctx, -1, seek_min, seek_target, seek_max, _pc->seek_flags);
            if (ret < 0) {      // Seek 失败，打印错误日志。
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", _pc->fmt_ctx->url);
            } else {
                // 清空 PacketQueue
                if (_pc->audio_stream >= 0)
                    _pc->audio_p_que.packet_queue_flush();
                if (_pc->subtitle_stream >= 0)
                    _pc->subtitle_p_que.packet_queue_flush();
                if (_pc->video_stream >= 0)
                    _pc->video_p_que.packet_queue_flush();

                // 重置外部时钟。如果是按字节 Seek，时钟设为 NaN；否则设为目标时间。
                if (_pc->seek_flags & AVSEEK_FLAG_BYTE) {
                    set_clock(&_pc->extclk, NAN, 0);
                } else {
                    set_clock(&_pc->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            _pc->seek_req = 0;       // 清除 Seek 请求标志。
            _pc->queue_attachments_req = 1;      // 标记需要重新处理封面图（如果有）。
            _pc->eof = 0;        // 重置文件结束标志（因为现在在文件中间）。
            if (_pc->paused)     // 如果当前处于暂停状态，强制刷新一帧画面，让显示立刻跳到新位置。
                step_to_next_frame(_pc);
        }

        // 处理附加图片
        if (_pc->queue_attachments_req) {
            if (_pc->video_st && _pc->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &_pc->video_st->attached_pic)) < 0)
                    goto fail;
                _pc->video_p_que.packet_queue_put(pkt);

                // 向 PacketQueue 发送空包
                _pc->video_p_que.packet_queue_put_nullpacket(pkt, _pc->video_stream);
            }
            _pc->queue_attachments_req = 0;
        }

        // 流量控制
        /* if the queue are full, no need to read more */
        if (_pc->audio_p_que.size + _pc->video_p_que.size + _pc->subtitle_p_que.size > MAX_QUEUE_SIZE      // 检查三个包队列的总字节数是否超过 15MB（MAX_QUEUE_SIZE）。
             || (stream_has_enough_packets(_pc->audio_st, _pc->audio_stream, &_pc->audio_p_que) &&
                 stream_has_enough_packets(_pc->video_st, _pc->video_stream, &_pc->video_p_que) &&
                 stream_has_enough_packets(_pc->subtitle_st, _pc->subtitle_stream, &_pc->subtitle_p_que))) {        // 或者，检查每个流是否已经拥有足够的数据包（MIN_FRAMES 且持续时间 > 1 秒）。
            /* wait 10 ms */
            // 加锁等待 10 毫秒，或者直到被 continue_read_thread 条件变量唤醒（如 Seek 或解码线程饥饿时）。
            std::unique_lock<std::mutex> locker(wait_mutex);
            _pc->continue_read_thread->wait_for(locker, std::chrono::milliseconds(10));

            continue;
        }

        // 检测播放结束（EOF）和循环（Loop）逻辑
        if (!_pc->paused &&      // 只有在非暂停状态下才检测是否结束。
            (!_pc->audio_st || (_pc->auddec.finished == _pc->audio_p_que.serial && _pc->audio_f_que.frame_queue_nb_remaining() == 0)) &&       // 如果没有音频流，忽略。如果有音频流：解码器已标记为 finished（即收到了 EOF 空包并排空了解码器） 并且 音频帧队列（sampq）中已经没有剩余帧了。
            (!_pc->video_st || (_pc->viddec.finished == _pc->video_p_que.serial && _pc->video_f_que.frame_queue_nb_remaining() == 0))) {       // 同理，对视频流也做同样的判断。如果以上条件都满足（文件确实播完了）：
            if (_pc->loop != 1 && (!_pc->loop || --_pc->loop)) {       // 如果 loop 变量不等于 1，且（loop 为 0 代表无限循环，或者 --loop 后大于 0）。loop 默认是 1。当 loop 为 0 时表示无限循环；当 loop > 1 时表示循环次数，每次减 1。
                stream_seek(_pc, _pc->start_time != AV_NOPTS_VALUE ? _pc->start_time : 0, 0, 0);       // 触发 Seek 到开头（或 _pc->start_time 指定的位置），实现循环播放。
            } else if (_pc->autoexit) {      // 如果循环次数用完，且开启了自动退出
                ret = AVERROR_EOF;      // 设置错误码为 EOF，跳转到 fail 标签，退出解封装线程。
                goto fail;
            }
        }

        // 读取一个 AVPacket
        ret = av_read_frame(fmt_ctx, pkt);

        // 读取错误 / EOF 处理
        if (ret < 0) {      // 读取失败。
            if ((ret == AVERROR_EOF || avio_feof(fmt_ctx->pb)) && !_pc->eof) {        // 如果是文件尾（EOF），且之前没有发过 EOF 信号。
                // 向所有活动的解码器队列发送 NULL 包。这是 ffplay 设计的精髓——NULL 包是解码器的“结束指令”，告诉解码器“没有更多数据了，把缓存里憋着的所有帧都吐出来”。
                if (_pc->video_stream >= 0)
                {
                    // 向 PacketQueue 发送空包
                    _pc->video_p_que.packet_queue_put_nullpacket(pkt, _pc->video_stream);
                }
                if (_pc->audio_stream >= 0)
                {
                    _pc->audio_p_que.packet_queue_put_nullpacket(pkt, _pc->audio_stream);
                }
                if (_pc->subtitle_stream >= 0)
                {
                    _pc->subtitle_p_que.packet_queue_put_nullpacket(pkt, _pc->subtitle_stream);
                }

                // 标记已经发送过 EOF 包，防止重复发送。
                _pc->eof = 1;
            }
            if (fmt_ctx->pb && fmt_ctx->pb->error) {      // 如果不是 EOF，而是真实的 I/O 错误（如网络断开、磁盘损坏）。
                if (_pc->autoexit)       // 如果开启了自动退出则报错，否则跳出循环（不再尝试读取）。
                    goto fail;
                else
                    break;
            }

            // 对于非致命错误，休眠 10ms 后重试（比如网络波动导致的一次性读取失败）。
            std::unique_lock<std::mutex> locker(wait_mutex);
            _pc->continue_read_thread->wait_for(locker, std::chrono::milliseconds(10));

            continue;
        } else {
            // 无报错
            _pc->eof = 0;
        }

        // 包分发 —— 将包放入对应的 PacketQueue
        // 检查包的 stream_index 是否等于音频/视频/字幕流索引
        if (pkt->stream_index == _pc->audio_stream) {
            _pc->audio_p_que.packet_queue_put(pkt);     // 满足条件的包调用 packet_queue_put 入队。
        } else if (pkt->stream_index == _pc->video_stream
                   && !(_pc->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {     // 视频流还要额外排除 AV_DISPOSITION_ATTACHED_PIC（因为封面图已经特殊处理过了，不能重复入队）。
            _pc->video_p_que.packet_queue_put(pkt);
        } else if (pkt->stream_index == _pc->subtitle_stream) {
            _pc->subtitle_p_que.packet_queue_put(pkt);
        } else {
            av_packet_unref(pkt);       // 如果是无关的流（如数据流、不关心的字幕流），立即释放该包，防止内存泄漏。
        }
    }

    // 循环结束与退出清理
    ret = 0;
fail:
    if (fmt_ctx && !_pc->fmt_ctx)      // 如果解封装上下文还存在且未被外部接管，则释放它。
    {
        avformat_close_input(&fmt_ctx);
        _pc->fmt_ctx = nullptr;
    }

    av_packet_free(&pkt);       // 释放临时工作包（pkt）。
    if (ret != 0) {     // 如果是异常退出（ret 为负值，如 AVERROR_EOF 或内存错误）。
        // 向主线程发送一个 FF_QUIT_EVENT 自定义事件，通知主线程程序需要退出或报错。
        // 可以由 QEvent 替代
    }

    return 0;
}

int VideoWidget::is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
        || !strcmp(s->iformat->name, "rtsp")
        || !strcmp(s->iformat->name, "sdp")
        )
        return 1;

    if(s->pb && (   !strncmp(s->url, "rtp:", 4)
                  || !strncmp(s->url, "udp:", 4)
                  )
        )
        return 1;
    return 0;
}

int VideoWidget::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue)
{
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

void VideoWidget::stream_seek(std::shared_ptr<PlayerCore> _pc, int64_t pos, int64_t rel, int by_bytes)
{
    if (!_pc->seek_req) {
        _pc->seek_pos = pos;
        _pc->seek_rel = rel;
        _pc->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (by_bytes)
            _pc->seek_flags |= AVSEEK_FLAG_BYTE;
        _pc->seek_req = 1;
        _pc->continue_read_thread->notify_one();
    }
}

void VideoWidget::set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

void VideoWidget::set_clock_at(Clock *c, double pts, int serial, double time)
{
    // pts 是音视频帧的 pts，来自码流；
    // last_update 是上一帧播放时刻，来自系统时钟；
    // pts_drift 是 pts 和 系统时钟的差值。
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

double VideoWidget::get_clock(Clock *c)
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

void VideoWidget::step_to_next_frame(std::shared_ptr<PlayerCore> _pc)
{
    /* if the stream is paused unpause it, then step */
    if (_pc->paused)
        stream_toggle_pause(_pc);
    _pc->step = 1;
}

void VideoWidget::stream_toggle_pause(std::shared_ptr<PlayerCore> _pc)
{
    if (_pc->paused) {
        _pc->frame_timer += av_gettime_relative() / 1000000.0 - _pc->vidclk.last_updated;
        if (_pc->read_pause_return != AVERROR(ENOSYS)) {
            _pc->vidclk.paused = 0;
        }
        set_clock(&_pc->vidclk, get_clock(&_pc->vidclk), _pc->vidclk.serial);
    }
    set_clock(&_pc->extclk, get_clock(&_pc->extclk), _pc->extclk.serial);
    _pc->paused = _pc->audclk.paused = _pc->vidclk.paused = _pc->extclk.paused = !_pc->paused;
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
    return 0;
}

int VideoWidget::decoder_start(Decoder *d, std::function<int (std::shared_ptr<PlayerCore>)> fn, std::shared_ptr<PlayerCore> _pc)
{
    packet_queue_start(d->queue);
    d->decoder_thread = std::thread(fn, _pc);
    return 0;
}

void VideoWidget::packet_queue_start(PacketQueue *queue)
{
    auto lock = queue->getLock();
    queue->abort_request = 0;
    queue->serial++;
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


