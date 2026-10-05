#include "board.h"
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/core/version.hpp>
#include <cmath>
#include <QHash>
#include <QSet>

QString BoardSpec::validate() const {
    if (int(type)<0 || int(type)>4) return QStringLiteral("未知图案类型。");
    if (cols < 2 || rows < 2 || cols > 40 || rows > 40)
        return QStringLiteral("行列数必须在 2–40 之间。");
    if (!std::isfinite(sizeMm) || sizeMm < 1 || sizeMm > 200)
        return QStringLiteral("单元尺寸必须在 1–200 mm 之间。");
    if (!std::isfinite(marginMm) || marginMm < 0 || marginMm > 200)
        return QStringLiteral("白边尺寸无效。");
    if (!std::isfinite(markerMm) || !std::isfinite(circleMm) || !std::isfinite(gapRatio) ||
        markerMm < 0.1 || markerMm > 190 || circleMm < 0.1 || circleMm > 190 || gapRatio < 0.1 || gapRatio > 1)
        return QStringLiteral("标记、圆点或间隙参数无效。");
    if (type == Pattern::Charuco && (!(markerMm > 0) || markerMm >= sizeMm * 0.85))
        return QStringLiteral("ChArUco 标记边长应小于方格边长的 85%。");
    if ((type == Pattern::Circles || type == Pattern::AsymmetricCircles) &&
        (!(circleMm > 0) || circleMm >= sizeMm))
        return QStringLiteral("圆点直径必须大于 0，且小于点间基本间距。");
    if (type == Pattern::AprilGrid && (cols * rows > 587 || gapRatio < 0.1 || gapRatio > 1.0))
        return QStringLiteral("AprilGrid 最多 587 个 tag，间隙比例须为 0.1–1.0。");
    return {};
}

QSizeF BoardSpec::patternSizeMm() const {
    switch (type) {
    case Pattern::Circles:
        return {(cols-1)*sizeMm+circleMm, (rows-1)*sizeMm+circleMm};
    case Pattern::AsymmetricCircles:
        // OpenCV object points: ((2*c + r%2)*spacing, r*spacing).
        return {(2*(cols-1)+1)*sizeMm+circleMm, (rows-1)*sizeMm+circleMm};
    case Pattern::AprilGrid:
        return {cols*sizeMm+(cols-1)*sizeMm*gapRatio,
                rows*sizeMm+(rows-1)*sizeMm*gapRatio};
    default: return {cols*sizeMm, rows*sizeMm};
    }
}
QSizeF BoardSpec::totalSizeMm() const {
    return patternSizeMm() + QSizeF(2*marginMm, 2*marginMm);
}
QString BoardSpec::description() const {
    const auto z = patternSizeMm();
    QString text = QStringLiteral("图案 %1 × %2 mm\n含白边 %3 × %4 mm")
        .arg(z.width(),0,'f',2).arg(z.height(),0,'f',2)
        .arg(totalSizeMm().width(),0,'f',2).arg(totalSizeMm().height(),0,'f',2);
    if (type == Pattern::Chessboard)
        text += QStringLiteral("\n%1 × %2 个方格 → %3 × %4 个内角点")
            .arg(cols).arg(rows).arg(cols-1).arg(rows-1);
    return text;
}
QString BoardSpec::kalibrYaml() const {
    QString header = QStringLiteral("# ScreenCalibrator: dimensions in meters.\n"
        "# Remeasure the displayed board before collecting data.\n");
    if (type == Pattern::Chessboard) {
        return header + QStringLiteral("target_type: checkerboard\ntargetCols: %1\ntargetRows: %2\n"
            "rowSpacingMeters: %3\ncolSpacingMeters: %3\n")
            .arg(cols-1).arg(rows-1).arg(sizeMm/1000,0,'g',12);
    }
    if (type == Pattern::AprilGrid)
        return header + QStringLiteral("target_type: aprilgrid\ntagCols: %1\ntagRows: %2\n"
            "tagSize: %3\ntagSpacing: %4\n")
            .arg(cols).arg(rows).arg(sizeMm/1000,0,'g',12).arg(gapRatio,0,'g',12);
    if (type == Pattern::Circles || type == Pattern::AsymmetricCircles)
        return header + QStringLiteral("target_type: circlegrid\ntargetCols: %1\ntargetRows: %2\n"
            "spacingMeters: %3\nasymmetricGrid: %4\n")
            .arg(cols).arg(rows).arg(sizeMm/1000,0,'g',12)
            .arg(type == Pattern::AsymmetricCircles ? "true" : "false");
    return {}; // Kalibr does not support ChArUco directly.
}

static QImage fromMat(const cv::Mat &m) {
    return QImage(m.data, m.cols, m.rows, int(m.step), QImage::Format_Grayscale8).copy();
}
QImage markerImage(int id, bool april) {
    static QHash<int,QImage> cache;
    int key=april ? 1000+id : id;
    if(cache.contains(key))return cache.value(key);
    static auto aprilDict=cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
    static auto charucoDict=cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_1000);
    auto dict = april ? aprilDict : charucoDict;
    cv::Mat m;
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 7)
    cv::aruco::generateImageMarker(dict, id, april ? 10 : 6, m, april ? 2 : 1);
#else
    cv::aruco::drawMarker(dict, id, april ? 10 : 6, m, april ? 2 : 1);
#endif
    // Kalibr's generator rotates the canonical tag by 180 degrees so that the
    // AprilTag corner convention matches its board object coordinates.
    auto image=fromMat(m);
    if(april)image=image.transformed(QTransform().rotate(180));
    cache.insert(key,image);return image;
}

static void paintMarker(QPainter &p,const QRectF &rect,const QImage &image) {
    // Draw each binary module as a vector rectangle. This preserves exact
    // marker dimensions in PDF and avoids any image interpolation in print.
    p.fillRect(rect,Qt::white);
    double w=rect.width()/image.width(),h=rect.height()/image.height();
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
        if(image.pixelColor(x,y).value()<128)
            p.fillRect(QRectF(rect.x()+x*w,rect.y()+y*h,w,h),Qt::black);
}

void paintBoard(QPainter &p, const BoardSpec &s) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const auto total = s.totalSizeMm();
    p.fillRect(QRectF(QPointF(0,0), total), Qt::white);
    p.translate(s.marginMm, s.marginMm);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    if (s.type == Pattern::Chessboard || s.type == Pattern::Charuco) {
        for (int r=0; r<s.rows; ++r)
            for (int c=0; c<s.cols; ++c)
                if ((r+c)%2 == 0) p.drawRect(QRectF(c*s.sizeMm,r*s.sizeMm,s.sizeMm,s.sizeMm));
        if (s.type == Pattern::Charuco) {
            // Obtain IDs and white-square positions from OpenCV, including its
            // version-dependent legacy chessboard convention.
            auto dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_1000);
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 7)
            cv::aruco::CharucoBoard board(cv::Size(s.cols,s.rows),float(s.sizeMm),float(s.markerMm),dict);
            auto objects = board.getObjPoints();
            auto ids = board.getIds();
#else
            auto board = cv::aruco::CharucoBoard::create(s.cols,s.rows,float(s.sizeMm),float(s.markerMm),dict);
            auto objects = board->objPoints;
            auto ids = board->ids;
#endif
            // Legacy OpenCV stores board Y upwards. Convert to screen Y
            // downwards, preserving the canonical marker corner ordering.
            for(auto &v:objects)
                if(v[2].y<v[0].y)
                    for(auto &corner:v)corner.y=float(s.rows*s.sizeMm)-corner.y;
            // Every white square contains a marker. Derive parity directly from
            // OpenCV's object coordinates, without rasterizing a large board.
            QSet<int> whiteSquares;
            for(const auto &v:objects) {
                int c=int(std::floor((v[0].x+s.markerMm/2)/s.sizeMm));
                int r=int(std::floor((v[0].y+s.markerMm/2)/s.sizeMm));
                whiteSquares.insert(r*s.cols+c);
            }
            p.fillRect(QRectF(0,0,s.cols*s.sizeMm,s.rows*s.sizeMm),Qt::white);
            for (int r=0;r<s.rows;++r)
                for (int c=0;c<s.cols;++c)
                    if(!whiteSquares.contains(r*s.cols+c))
                        p.drawRect(QRectF(c*s.sizeMm,r*s.sizeMm,s.sizeMm,s.sizeMm));
            for(size_t i=0;i<ids.size();++i) {
                const auto &v=objects[i];
                paintMarker(p,QRectF(v[0].x,v[0].y,s.markerMm,s.markerMm),markerImage(ids[i],false));
            }
        }
    } else if (s.type == Pattern::AprilGrid) {
        for (int r=0;r<s.rows;++r)
            for (int c=0;c<s.cols;++c) {
                // Kalibr places id 0 at the lower left and numbers along +x.
                const int id = (s.rows-1-r)*s.cols+c;
                paintMarker(p,QRectF(c*s.sizeMm*(1+s.gapRatio),r*s.sizeMm*(1+s.gapRatio),
                                  s.sizeMm,s.sizeMm),markerImage(id,true));
            }
        // Kalibr symmCorners: black squares at gap crossings, plus the outer
        // crossings in the existing margin. Metric board sizes stay unchanged.
        const double gap=s.sizeMm*s.gapRatio,pitch=s.sizeMm+gap;
        p.setClipRect(QRectF(-s.marginMm,-s.marginMm,total.width(),total.height()),Qt::IntersectClip);
        for(int r=0;r<=s.rows;++r)
            for(int c=0;c<=s.cols;++c)
                p.drawRect(QRectF(c*pitch-gap,r*pitch-gap,gap,gap));
    } else {
        p.setRenderHint(QPainter::Antialiasing, true);
        for (int r=0;r<s.rows;++r)
            for (int c=0;c<s.cols;++c) {
                const double x = (s.type==Pattern::AsymmetricCircles ? 2*c+r%2 : c)*s.sizeMm;
                p.drawEllipse(QRectF(x,r*s.sizeMm,s.circleMm,s.circleMm));
            }
    }
    p.restore();
}
