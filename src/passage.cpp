#include "passage.h"

#include <QColor>
#include <QFont>
#include <QRegularExpression>
#include <QTextBoundaryFinder>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>

namespace {
void parse(QTextDocument &document, const QString &source) {
    const auto dialect = QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub);
    document.setMarkdown(source, dialect | QTextDocument::MarkdownNoHTML);
}

QString imageLabel(const QTextCharFormat &format) {
    return format.stringProperty(QTextFormat::ImageAltText);
}
}

QList<SpokenSentence> spokenSentences(const QString &source) {
    QTextDocument document;
    parse(document, source);
    static const QRegularExpression symbols(
        QStringLiteral(R"([\x{2190}-\x{21ff}\x{27f0}-\x{27ff}\x{2900}-\x{297f}\x{2794}-\x{27be}]|<[-=]+>?|[-=]+>|[\x{0060}*_~#|]+)"));
    static const QRegularExpression htmlTags(QStringLiteral(R"(</?[A-Za-z][^<>]*>)"));
    static const QRegularExpression words(QStringLiteral("[\\p{L}\\p{N}]"));
    QList<SpokenSentence> sentences;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeLanguage))
            continue;
        QString paragraph;
        QList<int> positions;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            const auto format = fragment.charFormat();
            const auto original = fragment.text();
            const bool skip = format.fontFixedPitch() || (format.isAnchor() && original == format.anchorHref());
            const QString piece = skip ? QString(" ") : format.isImageFormat() ? imageLabel(format) : original;
            for (qsizetype i = 0; i < piece.size(); ++i)
                positions.append(fragment.position() + (format.isImageFormat() ? 0 : int(i)));
            paragraph += piece;
        }
        // Blank markup in place so sentence boundaries can map back to the formatted text.
        for (const auto &pattern : {htmlTags, symbols}) {
            for (const auto &match : pattern.globalMatch(paragraph)) {
                const qsizetype from = match.capturedStart();
                const qsizetype to = match.capturedEnd();
                for (qsizetype i = from; i < to; ++i)
                    paragraph[i] = ' ';
            }
        }
        QTextBoundaryFinder finder(QTextBoundaryFinder::Sentence, paragraph);
        qsizetype start = 0;
        for (qsizetype end = finder.toNextBoundary(); end >= 0; end = finder.toNextBoundary()) {
            while (start < end && paragraph.at(start).isSpace())
                ++start;
            qsizetype tail = end;
            while (tail > start && paragraph.at(tail - 1).isSpace())
                --tail;
            const QString sentence = paragraph.mid(start, tail - start).simplified();
            if (sentence.contains(words))
                sentences.append({sentence, positions.at(start), positions.at(tail - 1) + 1});
            start = end;
        }
    }
    return sentences;
}

int previewPosition(const QString &source, int position) {
    if (!source.contains("!["))
        return position;
    QTextDocument document;
    parse(document, source);
    int adjusted = position;
    for (auto block = document.begin(); block.isValid() && block.position() < position; block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (fragment.position() >= position)
                break;
            if (fragment.charFormat().isImageFormat())
                adjusted += int(imageLabel(fragment.charFormat()).size()) - fragment.length();
        }
    }
    return adjusted;
}

QString formattedPassage(const QString &source, const SpokenSentence *highlight, bool dark, bool fullscreen) {
    if (source.trimmed().isEmpty())
        return {};
    QTextDocument document;
    QFont font = document.defaultFont();
    font.setPixelSize(fullscreen ? 21 : 12);
    if (fullscreen) {
        font.setFamilies({"Noto Serif", "serif"});
        font.setStyleHint(QFont::Serif);
    }
    document.setDefaultFont(font);
    parse(document, source);
    if (fullscreen) {
        for (auto block = document.begin(); block.isValid(); block = block.next()) {
            auto format = block.blockFormat();
            const bool heading = format.headingLevel() > 0;
            format.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
            format.setTopMargin(heading && block != document.begin() ? 20 : 0);
            format.setBottomMargin(heading ? 10 : 16);
            QTextCursor(block).setBlockFormat(format);
        }
    }
    if (highlight) {
        QTextCursor cursor(&document);
        cursor.setPosition(highlight->start);
        cursor.setPosition(highlight->end, QTextCursor::KeepAnchor);
        QTextCharFormat active;
        active.setBackground(QColor(dark ? "#29486a" : "#dbeaff"));
        active.setForeground(QColor(dark ? "#eef3fb" : "#0f2a44"));
        cursor.mergeCharFormat(active);
    }
    // Keep alt text and remove image objects before exporting to the preview.
    struct Image { int position; int length; QString label; };
    QList<Image> images;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (fragment.charFormat().isImageFormat())
                images.append({fragment.position(), fragment.length(), imageLabel(fragment.charFormat())});
        }
    }
    QTextCursor cursor(&document);
    for (auto it = images.crbegin(); it != images.crend(); ++it) {
        cursor.setPosition(it->position);
        cursor.setPosition(it->position + it->length, QTextCursor::KeepAnchor);
        cursor.insertText(it->label, QTextCharFormat{});
    }
    return document.toHtml();
}
