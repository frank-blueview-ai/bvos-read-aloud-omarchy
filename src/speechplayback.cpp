#include "speechplayback.h"

#include <QIODevice>
#include <QMediaDevices>

namespace {
constexpr int sampleRate = 24000;
constexpr qsizetype monoSampleBytes = 2;
}

SpeechPlayback::SpeechPlayback(QObject *parent) : QObject(parent) {
    format.setSampleRate(sampleRate);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    pumpTimer.setInterval(25);
    connect(&pumpTimer, &QTimer::timeout, this, &SpeechPlayback::pump);
}

int SpeechPlayback::bytesPerSecond() const {
    return sampleRate * format.bytesPerFrame();
}

qint64 SpeechPlayback::positionBytes() const {
    const auto played = sink ? sink->processedUSecs() * bytesPerSecond() / 1000000 : 0;
    return qMin(offset, base + played);
}

double SpeechPlayback::position() const {
    return double(positionBytes()) / bytesPerSecond();
}

double SpeechPlayback::duration() const {
    return double(samples.size()) / bytesPerSecond();
}

void SpeechPlayback::begin() {
    clear();
    device = QMediaDevices::defaultAudioOutput();
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    if (!device.isFormatSupported(format))
        format.setChannelConfig(QAudioFormat::ChannelConfigMono);
    inputFinished = false;
    playing = true;
    emit changed();
}

void SpeechPlayback::append(const QByteArray &pcm) {
    const qsizetype size = pcm.size() - pcm.size() % monoSampleBytes;
    if (format.channelCount() == 2) {
        // Kokoro produces mono PCM. Write each sample to both stereo channels.
        const qsizetype start = samples.size();
        samples.resize(start + 2 * size);
        char *out = samples.data() + start;
        for (qsizetype i = 0; i < size; i += monoSampleBytes) {
            *out++ = pcm[i];
            *out++ = pcm[i + 1];
            *out++ = pcm[i];
            *out++ = pcm[i + 1];
        }
    } else {
        samples.append(pcm.constData(), size);
    }
    if (playing && !sink)
        startSink();
    pump();
    emit changed();
}

void SpeechPlayback::finishInput() {
    inputFinished = true;
    pump();
}

void SpeechPlayback::play() {
    if (playing || (!active() && !available()))
        return;
    playing = true;
    suspended = false;
    if (sink) {
        sink->resume();
        pumpTimer.start();
    } else {
        if (offset >= samples.size() && inputFinished)
            offset = base = 0;
        if (offset < samples.size())
            startSink();
    }
    pump();
    emit changed();
}

void SpeechPlayback::pause() {
    if (!playing)
        return;
    playing = false;
    suspended = true;
    pumpTimer.stop();
    if (sink)
        sink->suspend();
    emit changed();
}

void SpeechPlayback::skip(double seconds) {
    if (!available())
        return;
    const auto target = qBound<qint64>(0, positionBytes() + qRound64(seconds * bytesPerSecond()), samples.size());
    releaseSink();
    offset = base = target - target % format.bytesPerFrame();
    if (playing && offset < samples.size())
        startSink();
    pump();
    emit changed();
}

void SpeechPlayback::stop() {
    playing = false;
    suspended = false;
    inputFinished = true;
    releaseSink();
    offset = base = 0;
    emit changed();
}

void SpeechPlayback::clear() {
    stop();
    samples.clear();
    emit changed();
}

void SpeechPlayback::startSink() {
    if (device.isNull() || !device.isFormatSupported(format)) {
        stop();
        emit failed(tr("The default audio output cannot play the speech stream."));
        return;
    }
    sink = std::make_unique<QAudioSink>(device, format);
    connect(sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        QTimer::singleShot(0, this, [this, state] {
            if (sink && sink->state() == state)
                handleState(state);
        });
    });
    output = sink->start();
    if (output)
        pumpTimer.start();
    else {
        stop();
        emit failed(tr("Could not open the default audio output."));
    }
}

void SpeechPlayback::releaseSink() {
    pumpTimer.stop();
    output = nullptr;
    if (sink) {
        sink->disconnect(this);
        sink->reset();
        sink.reset();
    }
}

void SpeechPlayback::pump() {
    if (!playing)
        return;
    if (output && offset < samples.size()) {
        const auto available = qMin<qint64>(sink->bytesFree(), samples.size() - offset);
        const auto count = available - available % format.bytesPerFrame();
        if (count > 0) {
            const auto written = output->write(samples.constData() + offset, count);
            if (written < 0) {
                stop();
                emit failed(tr("Could not write to the default audio output."));
                return;
            }
            offset += written;
        }
    }
    if (inputFinished && offset == samples.size() && (!sink || sink->state() == QAudio::IdleState)) {
        base = offset;
        releaseSink();
        playing = false;
        emit finished();
    }
    emit changed();
}

void SpeechPlayback::handleState(QAudio::State state) {
    if (state == QAudio::StoppedState && sink->error() != QAudio::NoError) {
        stop();
        emit failed(tr("Could not play the response on the default audio output."));
    }
}
