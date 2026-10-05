#pragma once
#include "board.h"
#include "pdf_export.h"
#include "screen_metrics.h"
#include <QTabWidget>
#include <QWidget>
#include <QSettings>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <functional>

class Canvas : public QWidget {
public:
    BoardSpec spec;
    bool preview = true, ruler = false, printPreview = false;
    PrintSpec paper;
    double sx = 4, sy = 4;
    double lineX = 300, lineY = 300;
    QString error;
    std::function<void()> escaped;
    explicit Canvas(QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
};

class Window : public QWidget {
public:
    Window();
    ~Window() override { delete output; }
    void smoke(const QString &directory);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    QSettings settings;
    QComboBox *pattern, *screenChoice, *paperChoice;
    QTabWidget *tabs;
    QDoubleSpinBox *paperWidth, *paperHeight, *paperEdge;
    QLabel *printInfo;
    QPushButton *pdfButton;
    QSpinBox *cols, *rows;
    QDoubleSpinBox *size, *marker, *diameter, *gap, *margin;
    QLabel *info, *calibration;
    QPushButton *display;
    Canvas *preview, *output;
    QString outputFingerprint;
    double calibrationDpr = 1;
    BoardSpec spec() const;
    PrintSpec printSpec() const;
    ScaleEstimate effectiveScale() const;
    void useAutomatic();
    void exportPdf();
    QScreen *selectedScreen() const;
    QString fingerprint() const;
    void refresh();
    void placeOutput();
    void calibrate();
    void finishCalibration();
    void showBoard();
    void exportYaml();
    void savePreset();
    void loadPreset();
};
