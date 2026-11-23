#include "speechplayback.h"
#include "passage.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainterPath>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRegularExpression>
#include <QRegion>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>

namespace {
constexpr auto service = "ai.blueview.ReadAloud";
constexpr auto path = "/ReadAloud";
constexpr int maxText = 64000;

bool hasVoice(const QString &dir) {
    return QFileInfo(dir + "/venv/bin/python").isExecutable()
        && QFileInfo::exists(dir + "/read-aloud.py")
        && QFileInfo::exists(dir + "/kokoro-v1.0.onnx")
        && QFileInfo::exists(dir + "/voices-v1.0.bin");
}

QString voiceDirectory() {
    const QString override = qEnvironmentVariable("BLUE_VIEW_ASSET_DIR");
    if (!override.isEmpty())
        return override + "/speech";
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString shared = data + "/blue-view-local-chat/speech";
    return hasVoice(shared) ? shared : data + "/omarchy-read-aloud/speech";
}

QStringList clipboardWatchArgs(bool primary) {
    QStringList args{"--type", "text"};
    if (primary)
        args << "--primary";
    // wl-paste supplies the state and text from the same clipboard offer.
    return args << "--watch" << "sh" << "-c"
        << R"(if [ "$CLIPBOARD_STATE" = data ]; then head -c 256001 | base64 -w0; fi; printf '\n')";
}

QString clipboard(bool primary) {
    QProcess paste;
    const QDeadlineTimer deadline(1000);
    paste.start("wl-paste", clipboardWatchArgs(primary));
    while (!paste.canReadLine()) {
        const int remaining = deadline.remainingTime();
        if (remaining == 0 || !paste.waitForReadyRead(remaining))
            break;
    }
    const QByteArray frame = paste.canReadLine() ? paste.readLine().trimmed() : QByteArray();
    if (paste.state() != QProcess::NotRunning) {
        paste.kill();
        paste.waitForFinished();
    }
    return QString::fromUtf8(QByteArray::fromBase64(frame)).left(maxText + 1);
}
}

class Reader final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "ai.blueview.ReadAloud")
    Q_PROPERTY(QString preview READ preview NOTIFY previewChanged)
    Q_PROPERTY(int highlightPosition READ highlightPosition NOTIFY formattedPreviewChanged)
    Q_PROPERTY(QString formattedPreview READ formattedPreview NOTIFY formattedPreviewChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool playable READ playable NOTIFY changed)
    Q_PROPERTY(bool wantsPlayback READ wantsPlayback NOTIFY changed)
    Q_PROPERTY(bool speaking READ speaking NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(int sentence READ sentence NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(bool opacityAvailable READ opacityAvailable CONSTANT)
    Q_PROPERTY(int rounding READ rounding NOTIFY appearanceChanged)

public:
    Reader(const QString &copied, const QString &selected)
        : lastCopy(copied), lastSelection(selected) {
        connect(&clipboardWatcher, &QProcess::readyReadStandardOutput, this, [this] {
            while (clipboardWatcher.canReadLine()) {
                const QByteArray frame = clipboardWatcher.readLine().trimmed();
                const QString copied = QString::fromUtf8(QByteArray::fromBase64(frame));
                if (!copied.trimmed().isEmpty() && copied != lastCopy) {
                    lastCopy = copied;
                    load(copied, false);
                }
            }
        });
        connect(&player, &SpeechPlayback::changed, this, &Reader::changed);
        connect(&player, &SpeechPlayback::finished, this, [this] {
            if (index + 1 < passages.size())
                goTo(index + 1);
            else {
                completed = true;
                setHighlight(-1);
                emit changed();
            }
        });
        connect(&player, &SpeechPlayback::failed, this, &Reader::fail);
        connect(&worker, &QProcess::readyReadStandardOutput, this, &Reader::receive);
        connect(&worker, &QProcess::errorOccurred, this, [this](auto error) {
            if (error == QProcess::FailedToStart) {
                pending = false;
                fail(tr("Could not start the Blue View speech worker."));
            }
        });
        connect(&worker, &QProcess::finished, this, [this] {
            pending = false;
            fail(tr("The Blue View speech worker stopped."));
        });
        worker.setStandardErrorFile(QProcess::nullDevice());
        connect(&worker, &QProcess::started, this, &Reader::requestAudio);
        deadline.setSingleShot(true);
        deadline.setInterval(60000);
        connect(&deadline, &QTimer::timeout, &worker, &QProcess::kill);
        themePath = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state")
            + "/omarchy/current/theme/colors.toml";
        themeTimer.setInterval(1500);
        connect(&roundingQuery, &QProcess::finished, this, [this](int code) {
            const auto setting = QJsonDocument::fromJson(roundingQuery.readAllStandardOutput()).object().value("int");
            if (code == 0 && setting.isDouble() && setting.toInt() != cornerRounding) {
                cornerRounding = qMax(0, setting.toInt());
                emit appearanceChanged();
            }
        });
        connect(&themeTimer, &QTimer::timeout, this, &Reader::refreshAppearance);
        connect(&opacityQuery, &QProcess::finished, this, [this](int code) {
            if (code == 0)
                showOpacityPanel(opacityQuery.readAllStandardOutput());
        });
        themeTimer.start();
        refreshAppearance();
    }

    void watchClipboard() {
        clipboardWatcher.start("wl-paste", clipboardWatchArgs(false));
    }

    ~Reader() override {
        clipboardWatcher.disconnect(this);
        if (clipboardWatcher.state() != QProcess::NotRunning) {
            clipboardWatcher.terminate();
            if (!clipboardWatcher.waitForFinished(500)) {
                clipboardWatcher.kill();
                clipboardWatcher.waitForFinished();
            }
        }
        player.stop();
        worker.disconnect(this);
        if (worker.state() != QProcess::NotRunning) {
            worker.kill();
            worker.waitForFinished();
        }
    }

    QString preview() const { return previewText; }
    int highlightPosition() const {
        return highlightIndex < 0 ? -1 : previewPosition(previewText, passages.at(highlightIndex).start);
    }
    QString formattedPreview() const {
        return formattedPassage(previewText, highlightIndex >= 0 ? &passages.at(highlightIndex) : nullptr, darkTheme);
    }
    bool hasPreview() const { return editedPreview || !previewText.isEmpty(); }

    QString status() const {
        if (!message.isEmpty())
            return message;
        if (completed)
            return tr("Finished");
        if (player.available())
            return speaking() ? tr("Reading") : tr("Paused");
        return autoPlay ? tr("Preparing speech…") : (player.available() ? tr("Paused") : tr("Ready to read"));
    }
    bool speaking() const { return player.available() && player.active() && !player.paused(); }
    bool paused() const {
        return !autoPlay && !awaitingCopyStart && !completed && !passages.isEmpty() && message.isEmpty();
    }
    bool playable() const {
        return message.isEmpty() && (editedPreview ? !previewText.trimmed().isEmpty() : !passages.isEmpty());
    }
    bool wantsPlayback() const { return autoPlay && !completed; }
    int sentence() const { return passages.isEmpty() ? 0 : index + 1; }
    int total() const { return passages.size(); }
    bool dark() const { return darkTheme; }
    int rounding() const { return cornerRounding; }

    bool opacityAvailable() const {
        return QFileInfo::exists(QDir::homePath() + "/.config/omarchy/plugins/bvos.window-opacity/manifest.json");
    }

    Q_INVOKABLE void openOpacity() {
        if (opacityAvailable() && opacityQuery.state() == QProcess::NotRunning)
            opacityQuery.start("hyprctl", {"-j", "--batch", "clients; getoption decoration:active_opacity"});
    }

    Q_INVOKABLE void setCardInputArea(QWindow *window, int height, int radius) {
        const QRect bounds(0, 0, window->width(), height);
        if (radius == 0) {
            window->setMask(QRegion(bounds));
        } else {
            QPainterPath shape;
            shape.addRoundedRect(QRectF(bounds), radius, radius);
            window->setMask(QRegion(shape.toFillPolygon().toPolygon()));
        }
    }

    Q_INVOKABLE void editPreview(const QString &text) {
        if (text == previewText)
            return;
        if (!editedPreview) {
            ++generation;
            setHighlight(-1);
            passages.clear();
            index = 0;
            completed = false;
            autoPlay = false;
            awaitingCopyStart = false;
            message.clear();
            player.clear();
        }
        editedPreview = true;
        previewText = text;
        emit previewChanged();
        emit formattedPreviewChanged();
        emit changed();
    }

    Q_INVOKABLE void previous() {
        if (completed || index == 0 || player.position() > 2.0)
            goTo(index);
        else
            goTo(index - 1);
    }
    Q_INVOKABLE void next() {
        if (index + 1 < passages.size())
            goTo(index + 1);
    }
    Q_INVOKABLE void toggle() {
        Control(wantsPlayback() ? "pause" : "start");
    }

public slots:
    bool Control(const QString &action) {
        if (action == "start") {
            if (!playable())
                return false;
            if (editedPreview) {
                Read(previewText);
                return true;
            }
            autoPlay = true;
            awaitingCopyStart = false;
            if (completed)
                goTo(0);
            else if (player.available()) {
                setHighlight(index);
                player.play();
            }
        } else if (action == "pause") {
            if (playable() && !completed) {
                autoPlay = false;
                player.pause();
            }
        } else if (action == "previous") {
            previous();
        } else if (action == "next") {
            next();
        } else {
            return false;
        }
        emit changed();
        return true;
    }

    void Start() {
        if (paused() || editedPreview) {
            Control("start");
            return;
        }
        const QString copied = clipboard(false);
        const QString selected = clipboard(true);
        const bool newCopy = !copied.trimmed().isEmpty() && copied != lastCopy;
        const bool newSelection = !selected.trimmed().isEmpty() && selected != lastSelection;
        lastCopy = copied;
        lastSelection = selected;
        if (newCopy)
            Read(copied);
        else if (awaitingCopyStart && playable() && previewText == copied.trimmed())
            Control("start");
        else if (newSelection)
            Read(selected);
        else if (!Control("start"))
            Read(!selected.trimmed().isEmpty() ? selected : copied);
    }

    void Read(QString text) {
        load(text, true);
    }

signals:
    void changed();
    void previewChanged();
    void formattedPreviewChanged();
    void previewReplaced();
    void showCard();
    void appearanceChanged();

private:
    void load(const QString &text, bool play) {
        if (text.trimmed().isEmpty() && playable())
            return;
        ++generation;
        setHighlight(-1);
        passages.clear();
        message.clear();
        completed = false;
        autoPlay = play;
        awaitingCopyStart = !play;
        index = 0;
        player.clear();
        const QString clean = text.trimmed();
        const bool wasEdited = editedPreview;
        editedPreview = false;
        const QString nextPreview = clean.size() <= maxText ? clean : QString();
        if (previewText != nextPreview || wasEdited) {
            previewText = nextPreview;
            emit previewChanged();
        }
        emit formattedPreviewChanged();
        emit previewReplaced();
        if (clean.isEmpty())
            fail(tr("Select or copy some text first."));
        else if (clean.size() > maxText)
            fail(tr("Select fewer than 64,000 characters."));
        else {
            passages = spokenSentences(clean);
            QStringList prose;
            for (const auto &sentence : passages)
                prose.append(sentence.text);
            hint = prose.join(' ').left(1000);
            if (passages.isEmpty()) {
                fail(tr("No readable prose in this passage."));
            } else if (worker.state() == QProcess::NotRunning) {
                const QString dir = voiceDirectory();
                if (!hasVoice(dir)) {
                    fail(tr("Run bin/setup-voice to install Kokoro."));
                } else {
                    deadline.start();
                    worker.start(dir + "/venv/bin/python", {dir + "/read-aloud.py"});
                }
            } else {
                requestAudio();
            }
        }
        emit changed();
        if (play)
            emit showCard();
    }
    void fail(const QString &reason) {
        setHighlight(-1);
        if (!pending)
            deadline.stop();
        message = reason;
        emit changed();
    }

    void goTo(int target) {
        if (target < 0 || target >= passages.size() || !message.isEmpty())
            return;
        setHighlight(-1);
        index = target;
        completed = false;
        player.clear();
        requestAudio();
        emit changed();
    }

    void requestAudio() {
        if (pending || passages.isEmpty() || !message.isEmpty()
            || worker.state() != QProcess::Running)
            return;
        pending = true;
        requestedIndex = index;
        // Kokoro keeps one voice per generation. The surrounding text helps language detection.
        const QJsonObject request{{"id", generation}, {"text", passages.at(index).text},
                                  {"language", "auto"}, {"hint", hint}};
        worker.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
        deadline.start();
    }

    void receive() {
        while (worker.canReadLine()) {
            const auto response = QJsonDocument::fromJson(worker.readLine()).object();
            if (!response.value("id").isDouble())
                continue;
            pending = false;
            deadline.stop();
            if (response.value("id").toInt() != generation || requestedIndex != index) {
                requestAudio();
                continue;
            }
            const QByteArray pcm = QByteArray::fromBase64(response.value("audio").toString().toLatin1());
            if (pcm.isEmpty()) {
                fail(response.value("error") == "unsupported_language"
                         ? tr("Kokoro has no voice for this language.")
                         : tr("Kokoro could not read this sentence."));
                continue;
            }
            playAudio(pcm);
        }
    }

    void setHighlight(int sentence) {
        if (highlightIndex == sentence)
            return;
        highlightIndex = sentence;
        emit formattedPreviewChanged();
    }

    void playAudio(const QByteArray &pcm) {
        player.begin();
        if (!autoPlay)
            player.pause();
        if (autoPlay)
            setHighlight(index);
        player.append(pcm);
        player.finishInput();
    }

    void showOpacityPanel(const QByteArray &reply) {
        const auto separator = reply.lastIndexOf("\n{");
        if (separator < 0)
            return;
        const auto clients = QJsonDocument::fromJson(reply.first(separator)).array();
        const auto option = QJsonDocument::fromJson(reply.sliced(separator + 1)).object();
        const auto pid = QCoreApplication::applicationPid();
        const QString appClass = "omarchy-read-aloud";
        for (const auto &client : clients) {
            const auto window = client.toObject();
            if (window["pid"].toInteger() != pid || window["class"].toString() != appClass)
                continue;
            const QString address = window["address"].toString();
            if (address.isEmpty())
                return;
            QFile file(QDir::homePath() + "/.local/state/bvos/window-opacity.json");
            const auto saved = file.open(QIODevice::ReadOnly)
                ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
            const auto applications = saved["apps"].toObject();
            const auto instance = saved["windows"].toObject().value(address).toObject();
            const QString pidText = QString::number(pid);
            const int defaultValue = qRound(100 * option["float"].toDouble(1.0));
            const int appValue = applications.value(appClass).toInt(defaultValue);
            const int value = instance["pid"].toString() == pidText
                ? instance["value"].toInt(appValue) : appValue;
            QProcess::startDetached("omarchy-shell", {"bvos-window-opacity", "show", address,
                pidText, appClass, QString::number(value), applications.contains(appClass) ? "yes" : "no"});
            return;
        }
    }

    void refreshAppearance() {
        if (roundingQuery.state() == QProcess::NotRunning)
            roundingQuery.start("hyprctl", {"-j", "getoption", "decoration:rounding"});

        refreshTheme();
    }

    void refreshTheme() {
        QFile theme(themePath);
        if (!theme.open(QIODevice::ReadOnly))
            return;
        static const QRegularExpression backgroundPattern("^\\s*background\\s*=\\s*\"(#[0-9A-Fa-f]{6})\"",
                                                       QRegularExpression::MultilineOption);
        const QColor current(backgroundPattern.match(QString::fromUtf8(theme.readAll())).captured(1));
        const bool dark = current.isValid() ? current.lightnessF() < 0.5
            : QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        if (dark != darkTheme) {
            darkTheme = dark;
            emit formattedPreviewChanged();
            emit changed();
        }
    }

    SpeechPlayback player;
    QProcess worker;
    QProcess clipboardWatcher;
    QProcess roundingQuery;
    QProcess opacityQuery;
    QTimer deadline;
    QTimer themeTimer;
    QString themePath;
    bool darkTheme = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    QList<SpokenSentence> passages;
    int highlightIndex = -1;
    QString hint;
    QString previewText;
    bool editedPreview = false;
    QString lastCopy;
    QString lastSelection;
    QString message;
    int cornerRounding = 0;
    int generation = 0;
    int index = 0;
    int requestedIndex = 0;
    bool pending = false;
    bool completed = false;
    bool autoPlay = true;
    bool awaitingCopyStart = false;
};

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("omarchy-read-aloud");
    app.setApplicationDisplayName("BvOs Intelligence Read Aloud");
    app.setWindowIcon(QIcon(":/double-oval.svg"));
    app.setDesktopFileName("omarchy-read-aloud");
    const auto args = app.arguments();
    auto bus = QDBusConnection::sessionBus();
    const QString action = args.value(1).mid(2);
    const bool start = args.size() == 2 && action == "start";
    if (args.size() == 2 && QStringList{"pause", "previous", "next"}.contains(action)) {
        QDBusInterface running(service, path, service, bus);
        if (running.isValid()) {
            const QDBusReply<bool> result = running.call("Control", action);
            if (result.isValid() && result.value())
                return 0;
        }
        return 0;
    }
    QDBusInterface running(service, path, service, bus);
    if (start && running.isValid()) {
        const QDBusReply<void> result = running.call("Start");
        return result.isValid() ? 0 : 1;
    }
    const bool explicitText = args.size() == 3 && args.at(1) == "--text";
    const QString copied = explicitText ? QString() : clipboard(false);
    const QString selected = explicitText || args.contains("--clipboard") ? QString() : clipboard(true);
    QString text = !selected.trimmed().isEmpty() ? selected : copied;
    if (explicitText)
        text = args.at(2);
    else if (start && !copied.trimmed().isEmpty())
        text = copied;

    Reader reader(copied, selected);
    if (bus.isConnected()) {
        bus.registerObject(path, &reader, QDBusConnection::ExportAllSlots);
        if (!bus.registerService(service)) {
            QDBusInterface existing(service, path, service, bus);
            const QDBusReply<void> result = start ? existing.call("Start")
                                                 : existing.call("Read", text);
            return result.isValid() ? 0 : 1;
        }
    }
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("reader", &reader);
    engine.load(QUrl("qrc:/Player.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    reader.Read(text);
    reader.watchClipboard();
    return app.exec();
}

#include "main.moc"
