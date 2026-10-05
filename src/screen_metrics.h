#pragma once
#include <QByteArray>
#include <QSizeF>
#include <QString>
class QScreen;
struct ScaleEstimate {
    double sx=0,sy=0;
    QSizeF physicalMm;
    QString source,reason;
    bool valid() const {return sx>0 && sy>0;}
};
QSizeF edidPhysicalSize(const QByteArray &edid);
ScaleEstimate estimateScale(const QSizeF &logicalSize,const QSizeF &physicalMm,const QString &source);
ScaleEstimate detectScreenScale(QScreen *screen);
