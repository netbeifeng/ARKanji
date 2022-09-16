#pragma once

#include <iostream>

// Tesseract
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

// Json
#include <json/json.h>

#include "PoseEstimation.h"


#define DRAW_CONTOUR 0
#define DRAW_RECTANGLE 0

#define THICKNESS_VALUE 4

typedef std::vector<cv::Point> contour_t;
// List of contours
typedef std::vector<contour_t> contour_vector_t;

struct MyStrip {
    int stripeLength;
    int nStop;
    int nStart;
    cv::Point2f stripeVecX;
    cv::Point2f stripeVecY;
};

class Tracker {
    public:
        Tracker(tesseract::TessBaseAPI* api, Json::Value objs);
        cv::Mat track(cv::Mat frame, int threshold_value);

        std::map<int, std::vector<cv::Point2f>> getDetectedMarkerCorners();
        std::map<int, cv::Point2f>  getDetectedMarkerCenter();
        std::map<int, cv::Mat> getDetectedMarkerPose();
        void cleanDetectedMarkers();
        cv::Mat getMarkerPoseById(int id);
        cv::Point2f getMarkerCenterById(int id);
        std::vector<cv::Point2f> getMarkerCornersById(int id);

    private:
        std::map<int, cv::Scalar> detectedMarkerRotated;
        std::map<int, cv::Mat> detectedMarkers;
        std::map<int, std::vector<cv::Point2f>> detectedMarkerCorners;

        const int threshold_slider_max = 255;
        int threshold_slider = 0;
        const int fps = 30;
        tesseract::TessBaseAPI* api;
        Json::Value objs;

        // Get Center Point of 4 Corners
        cv::Point2f getCenterOfCorners(std::vector<cv::Point2f> corners) {
            return (corners[0] + corners[1] + corners[2] + corners[3]) / 4.0;
        }


        int subpixSampleSafe(const cv::Mat& pSrc, const cv::Point2f& p) {
            // Point is float, slide 14
            int fx = int(floorf(p.x));
            int fy = int(floorf(p.y));

            if (fx < 0 || fx >= pSrc.cols - 1 ||
                fy < 0 || fy >= pSrc.rows - 1)
                return 127;

            // Slides 15
            int px = int(256 * (p.x - floorf(p.x)));
            int py = int(256 * (p.y - floorf(p.y)));

            // Here we get the pixel of the starting point
            unsigned char* i = (unsigned char*)((pSrc.data + fy * pSrc.step) + fx);

            // Internsity, shift 3
            int a = i[0] + ((px * (i[1] - i[0])) >> 8);
            i += pSrc.step;
            int b = i[0] + ((px * (i[1] - i[0])) >> 8);

            // We want to return Intensity for the subpixel
            return a + ((py * (b - a)) >> 8);
        }

        cv::Mat calculate_Stripe(double dx, double dy, MyStrip& st) {
            // Norm (euclidean distance) from the direction vector is the length (derived from the Pythagoras Theorem)
            double diffLength = sqrt(dx * dx + dy * dy);

            // Length proportional to the marker size
            st.stripeLength = (int)(0.8 * diffLength);

            if (st.stripeLength < 5)
                st.stripeLength = 5;

            // Make stripeLength odd (because of the shift in nStop), Example 6: both sides of the strip must have the same length XXXOXXX
            st.stripeLength |= 1;

            // E.g. stripeLength = 5 --> from -2 to 2: Shift -> half top, the other half bottom
            st.nStop = st.stripeLength >> 1;
            //st.nStop = st.stripeLength / 2;
            st.nStart = -st.nStop;

            cv::Size stripeSize;

            // Sample a strip of width 3 pixels
            stripeSize.width = 3;
            stripeSize.height = st.stripeLength;

            // Normalized direction vector
            st.stripeVecX.x = dx / diffLength;
            st.stripeVecX.y = dy / diffLength;

            // Normalized perpendicular vector
            st.stripeVecY.x = st.stripeVecX.y;
            st.stripeVecY.y = -st.stripeVecX.x;

            // 8 bit unsigned char with 1 channel, gray
            return cv::Mat(stripeSize, CV_8UC1);
        }

        // Transfer Mat* to Pix* for Tesseract recognition
        Pix* mat8ToPix(cv::Mat* mat8)
        {
            Pix* pixd = pixCreate(mat8->size().width, mat8->size().height, 8);
            for (int y = 0; y < mat8->rows; y++) {
                for (int x = 0; x < mat8->cols; x++) {
                    pixSetPixel(pixd, x, y, (l_uint32)mat8->at<uchar>(y, x));
                }
            }
            return pixd;
        }
};