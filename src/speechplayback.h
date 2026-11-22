#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <memory>

class SpeechPlayback final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(double position READ position NOTIFY changed)
    Q_PROPERTY(double duration READ duration NOTIFY changed)

public:
    explicit SpeechPlayback(QObject *parent = nullptr);
    bool active() const { return playing || suspended; }
    bool paused() const { return suspended; }
    bool available() const { return !samples.isEmpty(); }
    double position() const;
    double duration() const;

    void begin();
    void append(const QByteArray &pcm);
    void finishInput();
    void stop();
    void clear();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void skip(double seconds);

signals:
    void changed();
    void finished();
    void failed(const QString &message);

private:
    void startSink();
    void releaseSink();
    void pump();
    void handleState(QAudio::State state);
    qint64 positionBytes() const;
    int bytesPerSecond() const;

    QAudioDevice device;
    QAudioFormat format;
    QByteArray samples;
    std::unique_ptr<QAudioSink> sink;
    QIODevice *output = nullptr;
    QTimer pumpTimer;
    qint64 offset = 0;
    qint64 base = 0;
    bool playing = false;
    bool suspended = false;
    bool inputFinished = true;
};
