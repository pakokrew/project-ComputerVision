#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>

#include "PixelClassifier.h"

using namespace std;
using namespace cv;

string dir = "RhobanVisionLog/log2/";
string ext = ".png";
string prefix = "";
int start = 100;
int end = 150;
int delay = 1000;

KalmanFilter KF(4, 2);
Point2f prevCenter(-1,-1);

void initKalmanInterpolation();
void KalmanInterpolation(Point2f center, bool isBallVisible, Mat out);
void fetchImages();
void process(Mat);

int main(int argc, char **argv) {

    if (argc < 4 || argc > 7){
        cout << "Usage: ComputerVision <image directory> <start image> <end image>" << endl
             << "                     [<image prefix> <image extension> <delay (ms)>]"
             << endl;
        return 0;
    }

    dir = argv[1];
    start = atoi(argv[2]);
    end = atoi(argv[3]);
    if (argc >= 5)
        prefix = argv[4];
    if (argc >= 6)
        ext = argv[5];
    if (argc >= 7)
        delay = atoi(argv[6]);

    initKalmanInterpolation();

    fetchImages();


    return 0;
}

// Initialise the Kalman filter
void initKalmanInterpolation(){

    // * * * * video tracking * * * * //
    setIdentity(KF.processNoiseCov, Scalar::all(1));
    setIdentity(KF.measurementNoiseCov, Scalar::all(1));
    // setIdentity(KF.errorCovPost, Scalar::all(1));

    setIdentity(KF.transitionMatrix);
    KF.transitionMatrix.at<float>(0,2) = 1;
    KF.transitionMatrix.at<float>(1,3) = 1;
}

// Kalman Interpolation of the ball center
void KalmanInterpolation(Point2f center, bool isBallVisible, Mat out){
    if (prevCenter.x < 0){
        if (isBallVisible){
            KF.measurementMatrix = (Mat_<float>(2, 4) << 1, 0, 0, 0,
                                    0, 1, 0, 0);

            KF.statePost.at<float>(0,0) = center.x;
            KF.statePost.at<float>(1,0) = center.y;
            KF.statePost.at<float>(2,0) = 0;
            KF.statePost.at<float>(3,0) = 0;
            prevCenter = center;
        }
    }else{
        setIdentity(KF.measurementNoiseCov, Scalar::all(100));

        Mat prediction;
        prediction = KF.predict();

        Mat observations_m(2,1, CV_32F);

        if (isBallVisible){
            observations_m.at<float>(0,0) = center.x;
            observations_m.at<float>(1,0) = center.y;
            prevCenter = center;
        }else{
            observations_m.at<float>(0,0) = prevCenter.x;
            observations_m.at<float>(1,0) = prevCenter.y;
        }

        Mat state(4,1, CV_32F);

        state = KF.correct(observations_m);

        Point po;
        po.x = state.at<float>(0,0);
        po.y = state.at<float>(1,0);

        circle(out, po, (int)8,Scalar(0,255,0) , 4, 8, 0 );
    }
}

// process all images
void fetchImages() {

    struct timeval startTime, endTime;

    for (int i = start; i <= end;) {
        stringstream ss;
        ss << dir << prefix << i << ext;
        string file = ss.str();

        // Init
        gettimeofday(&startTime, NULL);

        cout << ">> Processing image " << prefix << i << ext << "..." << endl;

        Mat image;
        image = imread(file);
        if(image.data == NULL) {
            cout << "Unable to load image" << endl;
            i++;
            continue;
        }

        process(image);

        // Result
        gettimeofday(&endTime, NULL);
        cout << "(Elapsed time for " << prefix << i << ext << " : "
             << (endTime.tv_sec - startTime.tv_sec) * 1000 +
                (endTime.tv_usec - startTime.tv_usec) / 1000
             << " ms)" << endl << endl;

        char k = (unsigned int)cvWaitKey(delay);
        if (k == 27) { // == ESC
            break;
        }else if(k == 81){ // <--
            if (i - 1 < start)
                continue;
            --i;
        }else{
            ++i;
        }
    }
}

// Display image in a window at the right of the previous windows
void show(const char *str, Mat image, int &offset){
    namedWindow(str);
    cvMoveWindow(str, offset, 0);
    offset += image.cols ;
    imshow(str, image);
}

// process the current image
void process(Mat image) {
    PixelClassifier pc;
    pc.setImage(image);

    int offset = 0;

    // * * * * base image * * * * //
    show("Base image", image, offset);

    // * * * * Filter Terrain * * * * //
    Mat imageOut;
    pc.generateImageFromClass(imageOut);
    show("Filtered Terrain", imageOut, offset);


    // common result image
    Mat out = image.clone();

    // * * * * BALL * * * * //
    Point2f center; float radius;
    bool isBallVisible = pc.detectBall(center, radius);
    if (isBallVisible){
        cout << "[BALL] Detected at " << center << " of radius " << radius << endl;
        circle(out, center, (int)radius * 3,Scalar(0,0,255) , 4, 8, 0 );
    }else{
        cout << "[BALL] Not detected" << endl;
    }

    // Video Tracking
    KalmanInterpolation(center, isBallVisible, out);


    // * * * * GOAL * * * * //
    vector<Point> goal;
    Point goalCenter;

    if(pc.detectGoal(goal, goalCenter)) {
        circle(out, goalCenter, 10,Scalar(255,0,0) , 20, 8, 0 );
        cout << "[GOAL] Detected at " << goalCenter << endl;
        pc.positionFromGoal(goal);
    }
    else {
        cout << "[GOAL] Not detected" << endl;
    }

    // output image
    show ("Result", out, offset);

}
