#include "window.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDir>
#include <QCryptographicHash>
#include <QScrollArea>
#include <algorithm>
#include <QSignalBlocker>
#include <cmath>

Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setMinimumSize(300,300);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
}
void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(),preview ? QColor("#e8edf2") : Qt::white);
    if (ruler) {
        lineX = std::min(400.0,double(width()-160));
        lineY = std::min(400.0,double(height()-180));
        p.setPen(QPen(Qt::black,2));
        const int x=80,y=90;
        p.drawLine(QPointF(x,y),QPointF(x+lineX,y));
        p.drawLine(QPointF(x,y-12),QPointF(x,y+12));
        p.drawLine(QPointF(x+lineX,y-12),QPointF(x+lineX,y+12));
        p.drawLine(QPointF(x,y+55),QPointF(x,y+55+lineY));
        p.drawLine(QPointF(x-12,y+55),QPointF(x+12,y+55));
        p.drawLine(QPointF(x-12,y+55+lineY),QPointF(x+12,y+55+lineY));
        p.setPen(Qt::black);
        p.drawText(QRect(30,15,width()-60,60),Qt::AlignLeft|Qt::TextWordWrap,
            QStringLiteral("用直尺分别测量两端刻线中心之间的横向、纵向距离（mm）。\n测量完按 Esc，输入结果。"));
        p.drawText(QPointF(x+20,y+55+lineY/2),QStringLiteral("纵向参考线"));
        p.drawText(QPointF(x+lineX/2-45,y+30),QStringLiteral("横向参考线"));
        return;
    }
    if(printPreview) {
        QString issue=paper.validate(spec);
        double scale=std::min((width()-40)/paper.widthMm,(height()-70)/paper.heightMm);
        p.translate((width()-paper.widthMm*scale)/2,(height()-paper.heightMm*scale)/2);
        p.scale(scale,scale);
        if(issue.isEmpty())paintPrintPage(p,spec,paper,true);
        else p.fillRect(QRectF(0,0,paper.widthMm,paper.heightMm),Qt::white);
        p.resetTransform();p.setPen(issue.isEmpty()?QColor("#475569"):QColor("#b91c1c"));
        p.drawText(rect().adjusted(25,25,-25,-40),Qt::AlignCenter|Qt::TextWordWrap,issue);
        p.drawText(QRect(10,height()-32,width()-20,25),Qt::AlignCenter,
            QStringLiteral("纸张布局预览 · 虚线为页边距，不导出到 PDF"));
        return;
    }
    QString problem = spec.validate();
    if (!error.isEmpty()) problem=error;
    const auto mm=spec.totalSizeMm();
    double xScale=sx,yScale=sy;
    if (preview) xScale=yScale=std::min((width()-40)/mm.width(),(height()-70)/mm.height());
    else if (mm.width()*sx>width() || mm.height()*sy>height())
        problem=QStringLiteral("目标板超出屏幕可显示范围。\n按 Esc 返回，减小尺寸或行列数。");
    if (!problem.isEmpty()) {
        p.setPen(QColor("#b91c1c"));
        p.drawText(rect().adjusted(30,30,-30,-30),Qt::AlignCenter|Qt::TextWordWrap,problem);
        return;
    }
    p.translate((width()-mm.width()*xScale)/2,(height()-mm.height()*yScale)/2);
    p.scale(xScale,yScale);
    paintBoard(p,spec);
    p.resetTransform();
    if (preview) {
        p.setPen(QColor("#475569"));
        p.drawText(QRect(10,height()-32,width()-20,25),Qt::AlignCenter,
                   QStringLiteral("布局预览 · 此区域的显示尺寸经过缩放"));
    }
}
void Canvas::keyPressEvent(QKeyEvent *event) {
    if (event->key()==Qt::Key_Escape) { hide(); auto callback=escaped;if (callback) callback(); }
    else QWidget::keyPressEvent(event);
}
void Window::closeEvent(QCloseEvent *event) {
    output->close();QWidget::closeEvent(event);
}

static QDoubleSpinBox *number(double value,double min,double max,int decimals=2) {
    auto *w=new QDoubleSpinBox;
    w->setRange(min,max);w->setDecimals(decimals);w->setValue(value);w->setSingleStep(1);
    return w;
}
Window::Window() : settings("Lichuang","ScreenCalibrator") {
    setWindowTitle(QStringLiteral("视觉工具箱 · 标定板"));
    resize(1200,900);
    auto *layout=new QHBoxLayout(this);
    auto *left=new QWidget;left->setFixedWidth(365);
    auto *controls=new QVBoxLayout(left);
    auto *title=new QLabel(QStringLiteral("视觉工具箱"));
    title->setStyleSheet("font-size: 25px; font-weight: 700;");controls->addWidget(title);
    auto *subtitle=new QLabel(QStringLiteral("标定板 · 屏幕实际尺寸 · 自定义纸张 PDF"));
    subtitle->setWordWrap(true);controls->addWidget(subtitle);
    auto *group=new QGroupBox(QStringLiteral("目标板参数"));
    auto *form=new QFormLayout(group);
    pattern=new QComboBox;pattern->addItems({QStringLiteral("棋盘格"),QStringLiteral("对称圆点阵"),
        QStringLiteral("非对称圆点阵"),QStringLiteral("ChArUco（4×4 / 1000）"),QStringLiteral("AprilGrid（36h11）")});
    cols=new QSpinBox;cols->setRange(2,40);cols->setValue(9);
    rows=new QSpinBox;rows->setRange(2,40);rows->setValue(7);
    size=number(20,1,200);size->setSuffix(" mm");
    marker=number(14,0.1,190);marker->setSuffix(" mm");
    diameter=number(8,0.1,190);diameter->setSuffix(" mm");
    gap=number(0.3,0.1,1,3);gap->setSingleStep(0.05);
    margin=number(20,0,200);margin->setSuffix(" mm");
    form->setVerticalSpacing(4);
    form->addRow(QStringLiteral("图案"),pattern);
    form->addRow(QStringLiteral("列数（方格/点/tag）"),cols);
    form->addRow(QStringLiteral("行数（方格/点/tag）"),rows);
    form->addRow(QStringLiteral("格边 / tag边 / 基本间距"),size);
    form->addRow(QStringLiteral("ChArUco 标记边长"),marker);
    form->addRow(QStringLiteral("圆点直径"),diameter);
    form->addRow(QStringLiteral("tag 间隙 / tag 边长"),gap);
    form->addRow(QStringLiteral("外围白边"),margin);
    controls->addWidget(group);
    tabs=new QTabWidget;
    auto *screenTab=new QWidget;auto *screenLayout=new QVBoxLayout(screenTab);
    screenChoice=new QComboBox;
    for (auto *s:QGuiApplication::screens())
        screenChoice->addItem(QStringLiteral("%1 · %2 × %3").arg(s->name()).arg(s->geometry().width()).arg(s->geometry().height()));
    screenLayout->addWidget(new QLabel(QStringLiteral("目标屏幕")));screenLayout->addWidget(screenChoice);
    calibration=new QLabel;calibration->setWordWrap(true);screenLayout->addWidget(calibration);
    auto *autoButton=new QPushButton(QStringLiteral("尝试自动检测 / 使用自动尺寸"));screenLayout->addWidget(autoButton);
    auto *calibrateButton=new QPushButton(QStringLiteral("用直尺校准屏幕…"));screenLayout->addWidget(calibrateButton);
    display=new QPushButton(QStringLiteral("全屏显示实际尺寸（Esc 返回）"));
    display->setMinimumHeight(42);display->setStyleSheet("QPushButton {background:#2563eb;color:white;border-radius:6px;padding:9px;} QPushButton:disabled {background:#94a3b8;}");screenLayout->addWidget(display);
    
    auto *printTab=new QWidget;auto *printLayout=new QVBoxLayout(printTab);
    auto *paperForm=new QFormLayout;
    paperChoice=new QComboBox;paperChoice->addItems({QStringLiteral("A4（横向 297 × 210 mm）"),QStringLiteral("A3（横向 420 × 297 mm）"),QStringLiteral("自定义")});
    paperWidth=number(settings.value("print/width",297).toDouble(),20,3000);paperWidth->setSuffix(" mm");
    paperHeight=number(settings.value("print/height",210).toDouble(),20,3000);paperHeight->setSuffix(" mm");
    paperEdge=number(settings.value("print/edge",10).toDouble(),0,500);paperEdge->setSuffix(" mm");
    if(paperWidth->value()!=297 || paperHeight->value()!=210)paperChoice->setCurrentIndex(2);
    auto *swapButton=new QPushButton(QStringLiteral("交换宽高（横 / 竖纸张）"));
    paperForm->addRow(QStringLiteral("纸张预设"),paperChoice);
    paperForm->addRow(QStringLiteral("纸张宽度"),paperWidth);paperForm->addRow(QStringLiteral("纸张高度"),paperHeight);
    paperForm->addRow(QStringLiteral("打印页边距"),paperEdge);printLayout->addLayout(paperForm);printLayout->addWidget(swapButton);
    printInfo=new QLabel;printInfo->setWordWrap(true);printLayout->addWidget(printInfo);
    pdfButton=new QPushButton(QStringLiteral("导出实际尺寸 PDF…"));pdfButton->setMinimumHeight(40);printLayout->addWidget(pdfButton);
    auto *printNote=new QLabel(QStringLiteral("使用上方目标板参数。板子在纸张上居中，不缩放。打印时选择“实际大小 / 100%”，再复测尺寸。"));
    printNote->setWordWrap(true);printLayout->addWidget(printNote);
    tabs->addTab(screenTab,QStringLiteral("屏幕显示"));tabs->addTab(printTab,QStringLiteral("PDF 打印"));controls->addWidget(tabs);
    auto *exportButton=new QPushButton(QStringLiteral("导出 Kalibr target.yaml…"));controls->addWidget(exportButton);
    auto *presetRow=new QHBoxLayout;
    auto *saveButton=new QPushButton(QStringLiteral("保存预设…"));auto *loadButton=new QPushButton(QStringLiteral("加载预设…"));
    presetRow->addWidget(saveButton);presetRow->addWidget(loadButton);controls->addLayout(presetRow);
    info=new QLabel;info->setWordWrap(true);controls->addWidget(info);
    controls->addStretch();
    preview=new Canvas;output=new Canvas;output->preview=false;
    output->setWindowTitle(QStringLiteral("标定板 · Esc 返回"));
    auto *scroll=new QScrollArea;scroll->setWidget(left);scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedWidth(382);
    layout->addWidget(scroll);layout->addWidget(preview,1);
    connect(pattern,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{refresh();});
    connect(screenChoice,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{output->hide();refresh();});
    for (auto *n:{cols,rows}) connect(n,qOverload<int>(&QSpinBox::valueChanged),this,[this]{refresh();});
    for (auto *n:{size,marker,diameter,gap,margin}) connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{refresh();});
    connect(calibrateButton,&QPushButton::clicked,this,[this]{calibrate();});
    connect(autoButton,&QPushButton::clicked,this,[this]{useAutomatic();});
    connect(tabs,&QTabWidget::currentChanged,this,[this]{refresh();});
    connect(pdfButton,&QPushButton::clicked,this,[this]{exportPdf();});
    connect(paperChoice,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){
        if(index==0){QSignalBlocker w(paperWidth),h(paperHeight);paperWidth->setValue(297);paperHeight->setValue(210);}
        if(index==1){QSignalBlocker w(paperWidth),h(paperHeight);paperWidth->setValue(420);paperHeight->setValue(297);}
        refresh();
    });
    for(auto *n:{paperWidth,paperHeight,paperEdge})connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,n]{
        if(n!=paperEdge){QSignalBlocker blocker(paperChoice);paperChoice->setCurrentIndex(2);}refresh();
    });
    connect(swapButton,&QPushButton::clicked,this,[this]{double w=paperWidth->value();paperWidth->setValue(paperHeight->value());paperHeight->setValue(w);refresh();});
    connect(display,&QPushButton::clicked,this,[this]{showBoard();});
    connect(exportButton,&QPushButton::clicked,this,[this]{exportYaml();});
    connect(saveButton,&QPushButton::clicked,this,[this]{savePreset();});
    connect(loadButton,&QPushButton::clicked,this,[this]{loadPreset();});
    connect(qApp,&QGuiApplication::screenAdded,this,[this](QScreen *){screenChoice->clear();for(auto *s:QGuiApplication::screens()) screenChoice->addItem(s->name());refresh();});
    connect(qApp,&QGuiApplication::screenRemoved,this,[this](QScreen *){output->hide();screenChoice->clear();for(auto *s:QGuiApplication::screens()) screenChoice->addItem(s->name());refresh();});
    auto *timer=new QTimer(this);timer->setInterval(700);
    connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start();
    refresh();
}
BoardSpec Window::spec() const {
    BoardSpec s;s.type=Pattern(pattern->currentIndex());s.cols=cols->value();s.rows=rows->value();
    s.sizeMm=size->value();s.markerMm=marker->value();s.circleMm=diameter->value();
    s.gapRatio=gap->value();s.marginMm=margin->value();return s;
}
QScreen *Window::selectedScreen() const {
    auto screens=QGuiApplication::screens();
    return screens.value(screenChoice->currentIndex(),QGuiApplication::primaryScreen());
}
QString Window::fingerprint() const {
    auto *s=selectedScreen();if(!s)return {};
    // Include scaling and display mode. A matching profile is only a remembered
    // measurement, not proof of panel identity if EDID has no serial number.
    QString text=s->name()+"|"+s->manufacturer()+"|"+s->model()+"|"+s->serialNumber()+
        QString("|%1x%2|%3|%4|%5|%6")
        .arg(s->geometry().width()).arg(s->geometry().height())
        .arg(s->devicePixelRatio(),0,'g',12).arg(s->physicalDotsPerInchX(),0,'g',12)
        .arg(s->physicalDotsPerInchY(),0,'g',12).arg(s->logicalDotsPerInch(),0,'g',12);
    return "screens/"+QString(QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256).toHex());
}
void Window::refresh() {
    auto s=spec();preview->spec=s;
    preview->printPreview=tabs->currentIndex()==1;preview->paper=printSpec();preview->update();
    const bool circles=s.type==Pattern::Circles || s.type==Pattern::AsymmetricCircles;
    marker->setEnabled(s.type==Pattern::Charuco);diameter->setEnabled(circles);gap->setEnabled(s.type==Pattern::AprilGrid);
    QString key=fingerprint();auto scale=effectiveScale();auto *screen=selectedScreen();
    bool manual=scale.source=="manual";
    QString summary=s.description();
    if(scale.valid() && screen) {
        if(manual)calibration->setText(QStringLiteral("使用已保存的直尺校准\nX %1 / Y %2 坐标单位/mm").arg(scale.sx,0,'f',4).arg(scale.sy,0,'f',4));
        else calibration->setText(QStringLiteral("自动估计：%1 × %2 mm\n来源：%3 · 尚未实测\n若实际尺寸不符，请用直尺校准。")
            .arg(scale.physicalMm.width(),0,'f',1).arg(scale.physicalMm.height(),0,'f',1).arg(scale.source));
        summary+=QStringLiteral("\n约 %1 × %2 物理像素/基本单元").arg(s.sizeMm*scale.sx*screen->devicePixelRatio(),0,'f',1).arg(s.sizeMm*scale.sy*screen->devicePixelRatio(),0,'f',1);
    } else calibration->setText(scale.reason.isEmpty()?QStringLiteral("自动检测未获得可用尺寸，请用直尺校准。"):scale.reason);
    display->setEnabled(scale.valid() && s.validate().isEmpty());
    if(!s.validate().isEmpty())summary+="\n"+s.validate();
    info->setText(summary);
    const auto paper=printSpec();const QString paperError=paper.validate(s);
    pdfButton->setEnabled(paperError.isEmpty());
    printInfo->setText(paperError.isEmpty()?QStringLiteral("可用区域 %1 × %2 mm\n目标板居中排版，按设置尺寸导出。")
        .arg(paper.widthMm-2*paper.edgeMm,0,'f',2).arg(paper.heightMm-2*paper.edgeMm,0,'f',2):paperError);
    printInfo->setStyleSheet(paperError.isEmpty()?"color:#475569;":"color:#b91c1c;");
    if(output->isVisible() && !output->ruler) {
        output->spec=s;
        if(!scale.valid() || outputFingerprint!=fingerprint() ||
            (manual && std::abs(output->devicePixelRatioF()-settings.value(key+"/windowDpr").toDouble())>0.001) ||
            output->windowHandle()->screen()!=selectedScreen())
            output->error=QStringLiteral("屏幕显示模式已改变，请重新检测或校准。\n按 Esc 返回。");
        else {
            output->sx=scale.sx;output->sy=scale.sy;
            double feature=s.type==Pattern::AprilGrid ? s.sizeMm/10 : s.type==Pattern::Charuco ? s.markerMm/6 :
                (circles ? s.circleMm : s.sizeMm);
            output->error=feature*std::min(output->sx,output->sy)*output->devicePixelRatioF()<3 ?
                QStringLiteral("图案细节小于 3 个物理像素，请增大单元尺寸。\n按 Esc 返回。") : QString();
        }
        output->update();
    }
}
void Window::placeOutput() {
    output->hide();output->winId();
    auto *screen=selectedScreen();
    output->windowHandle()->setScreen(screen);
    output->setGeometry(screen->geometry());output->showFullScreen();output->raise();output->activateWindow();output->setFocus();
}
void Window::calibrate() {
    output->ruler=true;output->error.clear();
    output->setCursor(Qt::ArrowCursor);
    outputFingerprint=fingerprint();
    output->escaped=[this]{finishCalibration();};placeOutput();
    QTimer::singleShot(150,this,[this]{calibrationDpr=output->devicePixelRatioF();});
}
void Window::finishCalibration() {
    if(outputFingerprint!=fingerprint() || std::abs(calibrationDpr-output->devicePixelRatioF())>0.001) {
        output->ruler=false;output->escaped={};
        QMessageBox::warning(this,QStringLiteral("请重新测量"),QStringLiteral("测量期间显示模式发生变化，请重新校准。"));refresh();return;
    }
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("输入直尺测量结果"));
    auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;
    auto *note=new QLabel(QStringLiteral("测量两端刻线中心的距离。横、纵方向都必须实测。"));note->setWordWrap(true);layout->addWidget(note);
    auto *x=number(100,1,2000,3),*y=number(100,1,2000,3);x->setSuffix(" mm");y->setSuffix(" mm");
    form->addRow(QStringLiteral("横向参考线实测长度"),x);form->addRow(QStringLiteral("纵向参考线实测长度"),y);layout->addLayout(form);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted) {
        if(outputFingerprint!=fingerprint()) {
            QMessageBox::warning(this,QStringLiteral("请重新测量"),QStringLiteral("显示模式已变化，请重新校准。"));
            output->ruler=false;output->escaped={};refresh();return;
        }
        QString key=fingerprint();settings.setValue(key+"/sx",output->lineX/x->value());
        settings.setValue(key+"/sy",output->lineY/y->value());
        settings.setValue(key+"/windowDpr",output->devicePixelRatioF());settings.setValue(key+"/preferAutomatic",false);settings.sync();
    }
    output->ruler=false;output->escaped={};refresh();
}
void Window::showBoard() {
    QString key=fingerprint();auto s=spec();
    if(!s.validate().isEmpty())return;
    output->spec=s;output->ruler=false;output->error.clear();output->escaped={};
    output->setCursor(Qt::BlankCursor);
    auto scale=effectiveScale();output->sx=scale.sx;output->sy=scale.sy;
    if(output->sx<=0 || output->sy<=0)return;
    placeOutput();
    outputFingerprint=key;
    QTimer::singleShot(150,this,[this,key]{
        if(!output->isVisible())return;
        if(output->windowHandle()->screen()!=selectedScreen() ||
           (effectiveScale().source=="manual" && std::abs(output->devicePixelRatioF()-settings.value(key+"/windowDpr").toDouble())>0.001))
            output->error=QStringLiteral("窗口缩放与校准时不同，请重新校准。\n按 Esc 返回。");
        const auto s=output->spec;
        double feature=s.type==Pattern::AprilGrid ? s.sizeMm/10 : s.type==Pattern::Charuco ? s.markerMm/6 :
            (s.type==Pattern::Circles || s.type==Pattern::AsymmetricCircles ? s.circleMm : s.sizeMm);
        if(feature*std::min(output->sx,output->sy)*output->devicePixelRatioF()<3)
            output->error=QStringLiteral("图案细节小于 3 个物理像素，请增大单元尺寸。\n按 Esc 返回。");
        output->update();
    });
}
PrintSpec Window::printSpec() const {
    PrintSpec p;p.widthMm=paperWidth->value();p.heightMm=paperHeight->value();p.edgeMm=paperEdge->value();return p;
}
ScaleEstimate Window::effectiveScale() const {
    auto key=fingerprint();double sx=settings.value(key+"/sx",0).toDouble(),sy=settings.value(key+"/sy",0).toDouble();
    if(!settings.value(key+"/preferAutomatic",false).toBool() && std::isfinite(sx) && std::isfinite(sy) && sx>0 && sy>0) {
        ScaleEstimate e;e.sx=sx;e.sy=sy;e.source="manual";return e;
    }
    return detectScreenScale(selectedScreen());
}
void Window::useAutomatic() {
    output->hide();settings.setValue(fingerprint()+"/preferAutomatic",true);settings.sync();refresh();
}
void Window::exportPdf() {
    auto board=spec();auto paper=printSpec();auto error=paper.validate(board);
    if(!error.isEmpty()){QMessageBox::warning(this,QStringLiteral("无法导出"),error);return;}
    QString path=QFileDialog::getSaveFileName(this,QStringLiteral("导出实际尺寸 PDF"),"calibration-board.pdf","PDF (*.pdf)");
    if(path.isEmpty())return;
    if(!path.endsWith(".pdf",Qt::CaseInsensitive))path+=".pdf";
    error=exportBoardPdf(path,board,paper);
    if(!error.isEmpty())QMessageBox::warning(this,QStringLiteral("导出失败"),error);
    else {
        settings.setValue("print/width",paper.widthMm);settings.setValue("print/height",paper.heightMm);settings.setValue("print/edge",paper.edgeMm);settings.sync();
        QMessageBox::information(this,QStringLiteral("PDF 已导出"),QStringLiteral("纸张 %1 × %2 mm，目标板按设置尺寸居中排版。\n打印请选择“实际大小 / 100%”，并用尺子复测。").arg(paper.widthMm).arg(paper.heightMm));
    }
}
void Window::exportYaml() {
    auto s=spec();QString yaml=s.kalibrYaml();
    if(!s.validate().isEmpty()) {QMessageBox::warning(this,QStringLiteral("参数无效"),s.validate());return;}
    if(yaml.isEmpty()) {QMessageBox::information(this,QStringLiteral("ChArUco"),QStringLiteral("Kalibr 不直接支持 ChArUco。请用 OpenCV 标定，并选择 DICT_4X4_1000；确保生成和检测使用相同的 OpenCV 版本/棋盘约定。"));return;}
    QString path=QFileDialog::getSaveFileName(this,QStringLiteral("导出目标板配置"),"target.yaml","YAML (*.yaml)");if(path.isEmpty())return;
    QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly) || f.write(yaml.toUtf8())<0 || !f.commit())QMessageBox::warning(this,QStringLiteral("保存失败"),f.errorString());
}
void Window::savePreset() {
    auto s=spec();QString path=QFileDialog::getSaveFileName(this,QStringLiteral("保存预设"),"board.json","JSON (*.json)");if(path.isEmpty())return;
    QJsonObject j{{"version",2},{"type",int(s.type)},{"cols",s.cols},{"rows",s.rows},{"size_mm",s.sizeMm},{"marker_mm",s.markerMm},{"circle_mm",s.circleMm},{"gap_ratio",s.gapRatio},{"margin_mm",s.marginMm}};
    auto paper=printSpec();j.insert("paper",QJsonObject{{"width_mm",paper.widthMm},{"height_mm",paper.heightMm},{"edge_mm",paper.edgeMm}});
    QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(QJsonDocument(j).toJson())<0||!f.commit())QMessageBox::warning(this,QStringLiteral("保存失败"),f.errorString());
}
void Window::loadPreset() {
    QString path=QFileDialog::getOpenFileName(this,QStringLiteral("加载预设"),{},"JSON (*.json)");if(path.isEmpty())return;
    QFile f(path);QJsonParseError error;
    if(!f.open(QIODevice::ReadOnly)){QMessageBox::warning(this,QStringLiteral("加载失败"),f.errorString());return;}
    auto doc=QJsonDocument::fromJson(f.readAll(),&error);auto j=doc.object();
    BoardSpec s;s.type=Pattern(j.value("type").toInt(-1));s.cols=j.value("cols").toInt();s.rows=j.value("rows").toInt();
    s.sizeMm=j.value("size_mm").toDouble();s.markerMm=j.value("marker_mm").toDouble();s.circleMm=j.value("circle_mm").toDouble();s.gapRatio=j.value("gap_ratio").toDouble();s.marginMm=j.value("margin_mm").toDouble();
    if(error.error!=QJsonParseError::NoError || !doc.isObject() || (j.value("version").toInt()!=1 && j.value("version").toInt()!=2) || int(s.type)<0 || int(s.type)>4 || !s.validate().isEmpty()){
        QMessageBox::warning(this,QStringLiteral("加载失败"),QStringLiteral("预设格式或参数无效。"));return;}
    if(j.value("version").toInt()==2) {
        auto paper=j.value("paper").toObject();double w=paper.value("width_mm").toDouble(),h=paper.value("height_mm").toDouble();
        double edge=paper.value("edge_mm").toDouble(-1);
        if(w<20 || w>3000 || h<20 || h>3000 || edge<0 || edge>500 || edge*2>=std::min(w,h)) {
            QMessageBox::warning(this,QStringLiteral("加载失败"),QStringLiteral("预设纸张参数无效。"));return;
        }
        paperWidth->setValue(w);paperHeight->setValue(h);paperEdge->setValue(edge);
    }
    pattern->setCurrentIndex(int(s.type));cols->setValue(s.cols);rows->setValue(s.rows);size->setValue(s.sizeMm);marker->setValue(s.markerMm);diameter->setValue(s.circleMm);gap->setValue(s.gapRatio);margin->setValue(s.marginMm);refresh();
}
void Window::smoke(const QString &directory) {
    QDir().mkpath(directory);show();QApplication::processEvents();grab().save(directory+"/interface.png");
    tabs->setCurrentIndex(1);refresh();QApplication::processEvents();grab().save(directory+"/print-interface.png");
    tabs->setCurrentIndex(0);
    for(int i=0;i<5;++i) {
        pattern->setCurrentIndex(i);cols->setValue(6);rows->setValue(5);refresh();QApplication::processEvents();
        preview->grab().save(directory+QString("/pattern-%1.png").arg(i));
    }
}
