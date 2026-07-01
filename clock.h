#ifndef CLOCK_H
#define CLOCK_H

typedef struct Clock {
    // 当前帧(待播放)显示时间戳，播放后，当前帧变成上一帧
    double pts;           /* clock base */
    // 当前帧显示时间戳与当前系统时钟时间的差值，即pts与当前时刻的差值
    double pts_drift;     /* clock base minus time at which we updated the clock */
    // 当前时钟(如视频时钟)最后一次更新时间，也即上一帧(视频帧/音频帧)播放时刻
    double last_updated;
    // 时钟速度控制，用于控制播放速度，仅用于外部时钟同步这种同步方式中，不关注
    double speed;
    // 播放序列，所谓播放序列就是一段连续的播放动作，一个seek操作会启动一段新的播放序列
    int serial;           /* clock is based on a packet with this serial */
    // 暂停标志
    int paused;
    // 指向packet_serial
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */

    // 这里有三个时钟相关的变量比较关键：
    // pts 是音视频帧的 pts，来自码流；
    // last_update 是上一帧播放时刻，来自系统时钟；
    // pts_drift 是 pts 和 系统时钟的差值，看下面的代码，pts_drift 实际就是 pts - last_updated 的值：
    // static void set_clock_at(Clock *c, double pts, int serial, double time)
    // {
    //     c->pts = pts;
    //     c->last_updated = time;
    //     c->pts_drift = c->pts - time;
    //     c->serial = serial;
    // }
} Clock;

#endif // CLOCK_H
