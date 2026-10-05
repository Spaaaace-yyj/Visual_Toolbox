#pragma once
#include "board.h"
struct PrintSpec {
    double widthMm=210,heightMm=297,edgeMm=10;
    QString validate(const BoardSpec &board) const;
    QPointF boardOriginMm(const BoardSpec &board) const;
};
void paintPrintPage(QPainter &painter,const BoardSpec &board,const PrintSpec &paper,bool guide=false);
QString exportBoardPdf(const QString &path,const BoardSpec &board,const PrintSpec &paper);
