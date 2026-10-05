#include "screen_metrics.h"
#include <QScreen>
#include <QDir>
#include <QFile>
#include <cmath>

QSizeF edidPhysicalSize(const QByteArray &edid) {
    if(edid.size()<128 || edid.left(8)!=QByteArray::fromHex("00ffffffffffff00"))return {};
    int checksum=0;for(int i=0;i<128;++i)checksum+=static_cast<unsigned char>(edid[i]);
    if((checksum&255)!=0)return {};
    auto byte=[&](int i){return int(static_cast<unsigned char>(edid[i]));};
    // Prefer the first detailed timing descriptor's millimeter dimensions.
    for(int offset=54;offset<=108;offset+=18) {
        if(byte(offset)==0 && byte(offset+1)==0)continue;
        int w=byte(offset+12)|((byte(offset+14)&0xf0)<<4);
        int h=byte(offset+13)|((byte(offset+14)&0x0f)<<8);
        if(w>0 && h>0)return {double(w),double(h)};
    }
    // Base-block display size is only precise to a centimeter.
    if(byte(21)>0 && byte(22)>0)return {double(byte(21)*10),double(byte(22)*10)};
    return {};
}
ScaleEstimate estimateScale(const QSizeF &logicalSize,const QSizeF &physicalMm,const QString &source) {
    ScaleEstimate e;e.source=source;e.physicalMm=physicalMm;
    double w=physicalMm.width(),h=physicalMm.height(),lw=logicalSize.width(),lh=logicalSize.height();
    if(!std::isfinite(w)||!std::isfinite(h)||!std::isfinite(lw)||!std::isfinite(lh)||w<40||h<30||w>3000||h>3000||lw<=0||lh<=0) {
        e.reason=QStringLiteral("未获得可信的屏幕物理尺寸，请用直尺校准。");return e;
    }
    double aspect=lw/lh;
    if(std::abs(std::log(aspect/(h/w)))<std::abs(std::log(aspect/(w/h))))std::swap(w,h);
    if(std::abs(std::log(aspect/(w/h)))>0.12) {
        e.reason=QStringLiteral("显示模式与物理尺寸的宽高比不一致，请用原生分辨率或直尺校准。");return e;
    }
    e.physicalMm={w,h};e.sx=lw/w;e.sy=lh/h;return e;
}
ScaleEstimate detectScreenScale(QScreen *screen) {
    if(!screen)return {};
    QString wanted=screen->name();wanted.replace("HDMI-A-","HDMI-");
    QStringList matches;
    QDir drm("/sys/class/drm");
    for(const QString &entry:drm.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
        QString name=entry;name.replace("HDMI-A-","HDMI-");
        if(name.endsWith("-"+wanted))matches.append(drm.filePath(entry+"/edid"));
    }
    if(matches.size()==1) {
        QFile file(matches.front());
        if(file.open(QIODevice::ReadOnly)) {
            auto mm=edidPhysicalSize(file.readAll());
            if(mm.width()>0 && mm.height()>0) {
                auto e=estimateScale(screen->geometry().size(),mm,QStringLiteral("显示器 EDID"));
                // A matching EDID with incompatible mode should require manual
                // measurement, rather than silently using a synthetic Qt DPI.
                return e;
            }
        }
    }
    return estimateScale(screen->geometry().size(),screen->physicalSize(),QStringLiteral("系统 / Qt 屏幕信息"));
}
