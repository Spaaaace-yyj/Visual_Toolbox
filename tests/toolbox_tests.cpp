#include "screen_metrics.h"
#include "pdf_export.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <iostream>
#include <stdexcept>
#include <cmath>
static void require(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    try {
        auto e=estimateScale({1920,1080},{480,270},"test");
        require(e.valid() && std::abs(e.sx-4)<1e-10 && std::abs(e.sy-4)<1e-10,"native screen estimate");
        e=estimateScale({1280,720},{480,270},"test");
        require(e.valid() && std::abs(e.sx-8.0/3)<1e-10,"fractional-scaled logical screen size");
        e=estimateScale({1080,1920},{480,270},"test");
        require(e.physicalMm==QSizeF(270,480),"portrait display rotates physical dimensions");
        require(!estimateScale({1920,1080},{0,0},"test").valid(),"missing physical size rejected");
        require(!estimateScale({1920,1080},{400,400},"test").valid(),"wrong screen aspect rejected");
        QByteArray edid(128,'\0');edid.replace(0,8,QByteArray::fromHex("00ffffffffffff00"));
        edid[54]=1;edid[66]=char(480&255);edid[67]=char(270&255);edid[68]=char(((480>>8)<<4)|(270>>8));
        int sum=0;for(int i=0;i<127;++i)sum+=static_cast<unsigned char>(edid[i]);edid[127]=char((-sum)&255);
        require(edidPhysicalSize(edid)==QSizeF(480,270),"EDID detailed millimeter size");
        edid[24]=1;require(edidPhysicalSize(edid).isEmpty(),"corrupt EDID rejected");
        BoardSpec board;PrintSpec paper;paper.widthMm=297;paper.heightMm=210;paper.edgeMm=10;
        require(paper.validate(board).isEmpty(),"A4 landscape fits default board");
        require(paper.boardOriginMm(board)==QPointF(38.5,15),"exact centered board position");
        paper.widthMm=210;paper.heightMm=297;
        require(!paper.validate(board).isEmpty(),"oversized board rejects portrait page without shrinking");
        QTemporaryDir temporary;
        QString directory=argc==2?QString::fromLocal8Bit(argv[1]):temporary.path();QDir().mkpath(directory);
        const QString invalid=directory+"/invalid.pdf";
        require(!exportBoardPdf(invalid,board,paper).isEmpty() && !QFile::exists(invalid),"invalid paper does not write PDF");
        paper.widthMm=297;paper.heightMm=210;
        auto result=exportBoardPdf(directory+"/chessboard-A4.pdf",board,paper);if(!result.isEmpty())std::cerr<<result.toStdString()<<"\n";require(result.isEmpty(),"export chessboard PDF");
        board.type=Pattern::Charuco;board.cols=8;board.rows=6;board.marginMm=15;
        paper.widthMm=320.5;paper.heightMm=240.25;
        require(exportBoardPdf(directory+"/charuco-custom.pdf",board,paper).isEmpty(),"export custom decimal-size paper");
        board.type=Pattern::AprilGrid;board.cols=6;board.rows=8;board.sizeMm=24;board.gapRatio=0.3;
        paper.widthMm=297;paper.heightMm=420;
        require(exportBoardPdf(directory+"/aprilgrid-A3.pdf",board,paper).isEmpty(),"export AprilGrid PDF");
        std::cout<<"PASS: EDID and screen estimates, rotation, paper bounds, PDF exports\n";
    }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
