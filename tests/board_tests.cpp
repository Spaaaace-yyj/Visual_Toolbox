#include "board.h"
#include <QApplication>
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <algorithm>
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 7)
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/charuco_detector.hpp>
#endif
#include <iostream>
#include <stdexcept>
#include <cmath>

static void require(bool condition,const char *msg) {if(!condition)throw std::runtime_error(msg);}
static void detect(const cv::Mat &image,int family,
                   std::vector<std::vector<cv::Point2f>> &corners,std::vector<int> &ids) {
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 7)
    cv::aruco::DetectorParameters params;
    params.markerBorderBits=family==cv::aruco::DICT_APRILTAG_36h11 ? 2 : 1;
    cv::aruco::ArucoDetector detector(cv::aruco::getPredefinedDictionary(static_cast<cv::aruco::PredefinedDictionaryType>(family)),params);
    detector.detectMarkers(image,corners,ids);
#else
    auto params=cv::aruco::DetectorParameters::create();
    params->markerBorderBits=family==cv::aruco::DICT_APRILTAG_36h11 ? 2 : 1;
    cv::aruco::detectMarkers(image,cv::aruco::getPredefinedDictionary(static_cast<cv::aruco::PREDEFINED_DICTIONARY_NAME>(family)),corners,ids,params);
#endif
}
static cv::Mat render(const BoardSpec &s,double scale=8,double yscale=0) {
    if(yscale==0)yscale=scale;
    auto z=s.totalSizeMm();
    QImage im(int(std::ceil(z.width()*scale)),int(std::ceil(z.height()*yscale)),QImage::Format_RGB888);
    im.fill(Qt::white);
    QPainter p(&im);p.scale(scale,yscale);paintBoard(p,s);p.end();
    cv::Mat rgb(im.height(),im.width(),CV_8UC3,im.bits(),im.bytesPerLine());cv::Mat gray;
    cv::cvtColor(rgb,gray,cv::COLOR_RGB2GRAY);return gray;
}
static void checkCharuco(BoardSpec s) {
    s.type=Pattern::Charuco;
    auto image=render(s);
    auto dictionary=cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_1000);
    std::vector<std::vector<cv::Point2f>> markerCorners,refCorners;
    std::vector<int> markerIds,refIds,charucoIds;
    std::vector<cv::Point2f> charucoCorners;
    cv::Mat reference;
    const cv::Size refSize(s.cols*160,s.rows*160);
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 7)
    cv::aruco::CharucoBoard board(cv::Size(s.cols,s.rows),float(s.sizeMm),float(s.markerMm),dictionary);
    board.generateImage(refSize,reference,0,1);
    auto objects=board.getObjPoints();auto chessCorners=board.getChessboardCorners();
    cv::aruco::CharucoDetector detector(board);
    detector.detectBoard(image,charucoCorners,charucoIds,markerCorners,markerIds);
#else
    auto board=cv::aruco::CharucoBoard::create(s.cols,s.rows,float(s.sizeMm),float(s.markerMm),dictionary);
    board->draw(refSize,reference,0,1);
    auto objects=board->objPoints;auto chessCorners=board->chessboardCorners;
    detect(image,cv::aruco::DICT_4X4_1000,markerCorners,markerIds);
    cv::aruco::interpolateCornersCharuco(markerCorners,markerIds,image,board,charucoCorners,charucoIds);
#endif
    require(markerIds.size()==objects.size(),"ChArUco all markers detected");
    detect(reference,cv::aruco::DICT_4X4_1000,refCorners,refIds);
    require(refIds.size()==markerIds.size(),"ChArUco matches official generator marker count");
    for(size_t i=0;i<markerIds.size();++i) {
        auto found=std::find(refIds.begin(),refIds.end(),markerIds[i]);
        require(found!=refIds.end(),"ChArUco matches official marker ID");
        size_t k=size_t(found-refIds.begin());
        for(int corner=0;corner<4;++corner)
            require(cv::norm(markerCorners[i][corner]-refCorners[k][corner]-cv::Point2f(float(s.marginMm*8),float(s.marginMm*8)))<2.5,
                    "ChArUco marker position/orientation matches official generator");
    }
    require(charucoIds.size()==chessCorners.size(),"ChArUco complete inner-corner detection");
    bool yUp=objects.front()[2].y<objects.front()[0].y;
    for(size_t i=0;i<charucoIds.size();++i) {
        const auto object=chessCorners.at(size_t(charucoIds[i]));
        cv::Point2f expected(float((s.marginMm+object.x)*8),
            float((s.marginMm+(yUp?s.rows*s.sizeMm-object.y:object.y))*8));
        require(cv::norm(charucoCorners[i]-expected)<1.5,"ChArUco corner IDs map to correct metric points");
    }
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    try {
        BoardSpec s;s.cols=9;s.rows=7;s.sizeMm=20;s.marginMm=20;
        require(s.totalSizeMm()==QSizeF(220,180),"chessboard physical bounds");
        require(s.kalibrYaml().contains("targetCols: 8"),"yaml uses inner corners");
        std::vector<cv::Point2f> corners;
        require(cv::findChessboardCorners(render(s),cv::Size(8,6),corners),"rendered chessboard detection");
        require(std::abs(cv::norm(corners[1]-corners[0])-160)<1,"chess square metric rendering");
        require(cv::findChessboardCorners(render(s,5.25,4.7),cv::Size(8,6),corners),"fractional scale detection");
        require(std::abs(cv::norm(corners[1]-corners[0])-105)<1 &&
                std::abs(cv::norm(corners[8]-corners[0])-94)<1,"independent X and Y metric scaling");
        s.type=Pattern::Circles;s.cols=6;s.rows=5;s.circleMm=8;
        require(s.patternSizeMm()==QSizeF(108,88),"circle bounds include radius at both ends");
        require(cv::findCirclesGrid(render(s),cv::Size(6,5),corners,cv::CALIB_CB_SYMMETRIC_GRID),"symmetric circle detection");
        s.type=Pattern::AsymmetricCircles;s.cols=4;s.rows=7;
        require(s.patternSizeMm()==QSizeF(148,128),"asymmetric staggered bounds");
        require(cv::findCirclesGrid(render(s),cv::Size(4,7),corners,cv::CALIB_CB_ASYMMETRIC_GRID|cv::CALIB_CB_CLUSTERING),"asymmetric circle detection");
        s.type=Pattern::AprilGrid;s.cols=4;s.rows=3;s.gapRatio=0.3;
        auto tag=markerImage(0,true);
        require(tag.width()==10,"Kalibr requires two-module AprilTag black border");
        // tag 0 and orientation from Kalibr's official target generator.
        const unsigned long long code=0xd5d628584ULL;
        for(int r=0;r<6;++r)for(int c=0;c<6;++c)
            require((tag.pixelColor(c+2,r+2).value()>128)==bool(code&(1ULL<<(35-r*6-c))),"Kalibr tag code orientation");
        require(s.patternSizeMm()==QSizeF(98,72),"april bounds exclude trailing gap");
        auto aprilImage=render(s);
        for(int r=0;r<=s.rows;++r)for(int c=0;c<=s.cols;++c) {
            const int x=int((s.marginMm+c*26-3)*8),y=int((s.marginMm+r*26-3)*8);
            require(aprilImage.at<unsigned char>(y,x)==0,"AprilGrid intersection and outer helper squares present");
        }
        require(aprilImage.at<unsigned char>(int((s.marginMm+10)*8),int((s.marginMm+23)*8))==255,
                "AprilGrid gap corridor stays white");
        std::vector<std::vector<cv::Point2f>> detected;std::vector<int> ids;
        detect(render(s),cv::aruco::DICT_APRILTAG_36h11,detected,ids);
        require(ids.size()==12,"aprilgrid all tags detect");
        for(size_t i=0;i<ids.size();++i) if(ids[i]==0) {
            float minX=detected[i][0].x,minY=detected[i][0].y;
            for(const auto &v:detected[i]){minX=std::min(minX,v.x);minY=std::min(minY,v.y);}
            require(std::abs(minX-160)<2 && std::abs(minY-576)<2,"tag zero at lower left");
            require(std::abs(cv::norm(detected[i][1]-detected[i][0])-160)<2,"april physical tag width");
        }
        s.type=Pattern::Charuco;s.cols=6;s.rows=5;s.markerMm=14;
        for(int columns:{6,7})for(int lines:{4,5}) {s.cols=columns;s.rows=lines;checkCharuco(s);}
        require(s.kalibrYaml().isEmpty(),"unsupported Kalibr target rejected");
        s.type=Pattern::AprilGrid;s.cols=40;s.rows=40;
        require(!s.validate().isEmpty(),"April dictionary capacity checked");
        s.type=Pattern::Chessboard;s.sizeMm=std::nan("");
        require(!s.validate().isEmpty(),"nonfinite dimensions rejected");
        std::cout<<"PASS: metric bounds, chess/circle/April/ChArUco detection and YAML conventions\n";
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
