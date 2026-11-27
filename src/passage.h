#pragma once

#include <QList>
#include <QString>

struct SpokenSentence {
    QString text;
    int start;
    int end;
};

struct PassageRange {
    int start;
    int end;
};

struct PassageHighlightRanges {
    PassageRange paragraph;
    QList<PassageRange> words;
};

PassageHighlightRanges passageHighlightRanges(const QString &source, const SpokenSentence &sentence);
QList<SpokenSentence> spokenSentences(const QString &source);
int previewPosition(const QString &source, int position);
QString formattedPassage(const QString &source, const PassageRange *highlight, bool dark, bool fullscreen = false);
