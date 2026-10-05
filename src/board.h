#pragma once
#include <QImage>
#include <QPainter>
#include <QSizeF>
#include <QString>

enum class Pattern { Chessboard, Circles, AsymmetricCircles, Charuco, AprilGrid };
struct BoardSpec {
    Pattern type = Pattern::Chessboard;
    int cols = 9; // squares for chess/ChArUco, dots or tags otherwise
    int rows = 7;
    double sizeMm = 20.0; // square/tag edge; circle grid base spacing
    double markerMm = 14.0;
    double circleMm = 8.0;
    double gapRatio = 0.3;
    double marginMm = 20.0;
    QString validate() const;
    QSizeF patternSizeMm() const;
    QSizeF totalSizeMm() const;
    QString kalibrYaml() const;
    QString description() const;
};

// All geometry is specified in millimeters. Scale maps mm to Qt logical pixels.
// The desktop compositor and the panel determine final pixel quantization.
void paintBoard(QPainter &painter, const BoardSpec &spec);
QImage markerImage(int id, bool april);
