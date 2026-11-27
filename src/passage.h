#pragma once

#include <QList>
#include <QString>

struct SpokenSentence {
    QString text;
    int start;
    int end;
};

QList<SpokenSentence> spokenSentences(const QString &source);
int previewPosition(const QString &source, int position);
QString formattedPassage(const QString &source, const SpokenSentence *highlight, bool dark, bool fullscreen = false);
