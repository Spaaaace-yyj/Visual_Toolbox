#include "pdf_export.h"
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QSaveFile>
#include <QBuffer>
#include <QRegularExpression>
#include <array>
#include <cmath>
QString PrintSpec::validate(const BoardSpec &board) const {
    if(!board.validate().isEmpty())return board.validate();
    if(!std::isfinite(widthMm)||!std::isfinite(heightMm)||widthMm<20||heightMm<20||widthMm>3000||heightMm>3000)
        return QStringLiteral("纸张宽高须在 20–3000 mm 之间。");
    if(!std::isfinite(edgeMm)||edgeMm<0 || 2*edgeMm>=std::min(widthMm,heightMm))
        return QStringLiteral("页边距须小于纸张短边的一半。");
    auto mm=board.totalSizeMm();
    if(mm.width()>widthMm-2*edgeMm+1e-8 || mm.height()>heightMm-2*edgeMm+1e-8)
        return QStringLiteral("目标板含白边 %1 × %2 mm，超出纸张可用区域 %3 × %4 mm。请调整纸张或棋盘参数。")
            .arg(mm.width(),0,'f',2).arg(mm.height(),0,'f',2).arg(widthMm-2*edgeMm,0,'f',2).arg(heightMm-2*edgeMm,0,'f',2);
    return {};
}
QPointF PrintSpec::boardOriginMm(const BoardSpec &board) const {
    auto mm=board.totalSizeMm();return {(widthMm-mm.width())/2,(heightMm-mm.height())/2};
}
void paintPrintPage(QPainter &p,const BoardSpec &board,const PrintSpec &paper,bool guide) {
    p.save();p.fillRect(QRectF(0,0,paper.widthMm,paper.heightMm),Qt::white);
    if(guide) {
        QPen pen(QColor("#94a3b8"));pen.setWidthF(0.3);pen.setStyle(Qt::DashLine);
        p.setPen(pen);p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(paper.edgeMm,paper.edgeMm,paper.widthMm-2*paper.edgeMm,paper.heightMm-2*paper.edgeMm));
    }
    p.translate(paper.boardOriginMm(board));paintBoard(p,board);p.restore();
}
static QString exactPageSize(QByteArray &pdf,const PrintSpec &paper) {
    // Qt 5 rounds paper dimensions to whole PDF points. Preserve the drawing
    // scale and correct the page box + top-origin using a standards-compliant
    // incremental update, without rewriting compressed drawing streams.
    QString text=QString::fromLatin1(pdf.constData(),pdf.size());
    QRegularExpression pageExpression("(\\d+) 0 obj\\s*(<<\\s*/Type /Page\\b.*?>>)\\s*endobj",QRegularExpression::DotMatchesEverythingOption);
    auto page=pageExpression.match(text);
    auto box=QRegularExpression("/MediaBox \\[([^]]+)\\]").match(page.captured(2));
    auto contents=QRegularExpression("/Contents (\\d+) 0 R").match(page.captured(2));
    int trailerIndex=text.lastIndexOf("trailer");QString trailer=text.mid(trailerIndex);
    auto size=QRegularExpression("/Size (\\d+)").match(trailer);
    auto root=QRegularExpression("/Root (\\d+ \\d+ R)").match(trailer);
    auto info=QRegularExpression("/Info (\\d+ \\d+ R)").match(trailer);
    auto previous=QRegularExpression("startxref\\s+(\\d+)").match(trailer);
    if(!page.hasMatch()||!box.hasMatch()||!contents.hasMatch()||!size.hasMatch()||!root.hasMatch()||!previous.hasMatch())
        return QStringLiteral("PDF 页面结构不受支持，无法保证自定义纸张尺寸。");
    auto numbers=box.captured(1).split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
    if(numbers.size()!=4)return QStringLiteral("PDF 页面尺寸结构无效。");
    double oldHeight=numbers[3].toDouble()-numbers[1].toDouble();
    double width=paper.widthMm*72/25.4,height=paper.heightMm*72/25.4;
    int pageId=page.captured(1).toInt(),prefixId=size.captured(1).toInt(),suffixId=prefixId+1;
    QString dictionary=page.captured(2);
    dictionary.replace(box.captured(0),QString("/MediaBox [0 0 %1 %2]").arg(width,0,'f',10).arg(height,0,'f',10));
    dictionary.replace(contents.captured(0),QString("/Contents [%1 0 R %2 0 R %3 0 R]").arg(prefixId).arg(contents.captured(1)).arg(suffixId));
    std::array<qint64,3> offsets;
    offsets[0]=pdf.size();pdf+=QByteArray::number(pageId)+" 0 obj\n"+dictionary.toLatin1()+"\nendobj\n";
    QByteArray prefix=QString("q\n1 0 0 1 0 %1 cm\n").arg(height-oldHeight,0,'f',10).toLatin1();
    offsets[1]=pdf.size();pdf+=QByteArray::number(prefixId)+" 0 obj\n<< /Length "+QByteArray::number(prefix.size())+" >>\nstream\n"+prefix+"endstream\nendobj\n";
    QByteArray suffix="Q\n";offsets[2]=pdf.size();pdf+=QByteArray::number(suffixId)+" 0 obj\n<< /Length 2 >>\nstream\n"+suffix+"endstream\nendobj\n";
    qint64 xref=pdf.size();
    auto entry=[](qint64 offset){return QByteArray::number(offset).rightJustified(10,'0')+" 00000 n \n";};
    pdf+="xref\n0 1\n0000000000 65535 f \n"+QByteArray::number(pageId)+" 1\n"+entry(offsets[0]);
    pdf+=QByteArray::number(prefixId)+" 2\n"+entry(offsets[1])+entry(offsets[2]);
    pdf+="trailer\n<< /Size "+QByteArray::number(prefixId+2)+" /Root "+root.captured(1).toLatin1();
    if(info.hasMatch())pdf+=" /Info "+info.captured(1).toLatin1();
    pdf+=" /Prev "+previous.captured(1).toLatin1()+" >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
    return {};
}
QString exportBoardPdf(const QString &path,const BoardSpec &board,const PrintSpec &paper) {
    auto error=paper.validate(board);if(!error.isEmpty())return error;
    QByteArray pdf;QBuffer buffer(&pdf);buffer.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&buffer);
        writer.setCreator("Visual Toolbox 0.2.0");writer.setTitle("Calibration board - print at actual size (100%)");
        writer.setPdfVersion(QPagedPaintDevice::PdfVersion_1_4);
        writer.setResolution(1200);
        QPageSize size(QSizeF(paper.widthMm,paper.heightMm),QPageSize::Millimeter,"Custom",QPageSize::ExactMatch);
        QPageLayout layout(size,QPageLayout::Portrait,QMarginsF(0,0,0,0),QPageLayout::Millimeter);
        layout.setMode(QPageLayout::FullPageMode);
        if(!writer.setPageLayout(layout))return QStringLiteral("无法设置 PDF 纸张尺寸。");
        QPainter painter;
        if(!painter.begin(&writer))return QStringLiteral("无法初始化 PDF 绘制。");
        painter.scale(writer.resolution()/25.4,writer.resolution()/25.4);
        paintPrintPage(painter,board,paper);
        if(!painter.end())return QStringLiteral("PDF 绘制未能正常结束。");
    }
    buffer.close();error=exactPageSize(pdf,paper);if(!error.isEmpty())return error;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return file.errorString();
    if(file.write(pdf)!=pdf.size() || !file.commit())return file.errorString();
    return {};
}
