#include "Tracker.h"

Tracker::Tracker(tesseract::TessBaseAPI* para_api, Json::Value para_objs) {
	api = para_api;
	objs = para_objs;
}

cv::Mat Tracker::track(cv::Mat frame, int threshold_value) {
	// Result Pose (RT)
	float resultMatrix[16];
	resultMatrix[0] = -1;

	// Clone frame for tracking
	cv::Mat imgFiltered = frame.clone();

	// Convert to gray scale image
	cv::Mat grayScale;
	cv::cvtColor(imgFiltered, grayScale, cv::COLOR_BGR2GRAY);

	// Thresholding for distinct contrast
	cv::threshold(grayScale, grayScale, threshold_value, 255, cv::THRESH_BINARY);

	// OpenCV function for finding contours inside BW-image
	contour_vector_t contours;
	cv::findContours(grayScale, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

	// For each found Contour
	for (size_t k = 0; k < contours.size(); k++) {

		contour_t approx_contour;

		// Simplifying of the contour with the Ramer-Douglas-Peuker Algorithm
		// true -> Only closed contours
		// Approxicv::Mation of old curve, the difference (epsilon) should not be bigger than: perimeter(->arcLength)*0.02
		cv::approxPolyDP(contours[k], approx_contour, arcLength(contours[k], true) * 0.02, true);
		
		cv::Scalar colour;
		// 4 Corners => We color them
		if (approx_contour.size() == 4) {
			colour = cv::Scalar(0, 0, 255);
		}
		else {
			// If the approximated contours doesn't have 4 corners => No need to continue
			continue;
		}

		// Convert to a usable rectangle
		cv::Rect r = cv::boundingRect(approx_contour);
		// Filter tiny ones, if the found contour is too small 
		// (20 -> pixels, frame.cols - 10 to prevent extreme big contours)
		if (r.height < 20 || r.width < 20 || r.width > imgFiltered.cols - 10 || r.height > imgFiltered.rows - 10) {
			continue;
		}

		// Draw Founded potential markers in OpenCV
		// 1 -> 1 contour, we have a closed contour, true -> closed, 4 -> thickness
		cv::polylines(imgFiltered, approx_contour, true, colour, THICKNESS_VALUE);



		// Direction vector (x0,y0) and contained point (x1,y1) -> For each line -> 4x4 = 16
		float lineParams[16];
		// lineParams is shared, CV_32F -> Same data type like lineParams
		cv::Mat lineParamsMat(cv::Size(4, 4), CV_32F, lineParams);

		// For each corner point
		for (size_t i = 0; i < approx_contour.size(); ++i) {
			// Render the corners, 3 -> Radius, -1 filled circle
			cv::circle(imgFiltered, approx_contour[i], 3, CV_RGB(0, 255, 0), -1);

			// Euclidic distance, 7 -> parts, both directions dx and dy
			// Direction between two corner points
			double dx = ((double)approx_contour[(i + 1) % 4].x - (double)approx_contour[i].x) / 7.0;
			double dy = ((double)approx_contour[(i + 1) % 4].y - (double)approx_contour[i].y) / 7.0;

			MyStrip strip;
			cv::Mat imagePixelStripe = calculate_Stripe(dx, dy, strip);

			// Array for edge point centers
			cv::Point2f edgePointCenters[6];

			// First point already rendered, now the other 6 points
			// It means for each edge, we find 7 points on the edge!
			for (int j = 1; j < 7; ++j) {
				// Position calculation
				double px = (double)approx_contour[i].x + (double)j * dx;
				double py = (double)approx_contour[i].y + (double)j * dy;

				cv::Point p;
				p.x = (int)px;
				p.y = (int)py;
				cv::circle(imgFiltered, p, 2, CV_RGB(0, 0, 255), -1);

				// Columns: Loop over 3 pixels
				for (int m = -1; m <= 1; ++m) {
					// Rows: From bottom to top of the stripe, e.g. -3 to 3
					for (int n = strip.nStart; n <= strip.nStop; ++n) {
						cv::Point2f subPixel;

						// m -> going over the 3 pixel thickness of the stripe, n -> over the length of the stripe, direction comes from the orthogonal vector in st
						// Going from bottom to top and defining the pixel coordinate for each pixel belonging to the stripe
						subPixel.x = (double)p.x + ((double)m * strip.stripeVecX.x) + ((double)n * strip.stripeVecY.x);
						subPixel.y = (double)p.y + ((double)m * strip.stripeVecX.y) + ((double)n * strip.stripeVecY.y);

						cv::Point p2;
						p2.x = (int)subPixel.x;
						p2.y = (int)subPixel.y;

						// The one (purple color) which is shown in the stripe window
						//if (isFirstStripe)
						//	circle(imgFiltered, p2, 1, CV_RGB(255, 0, 255), -1);
						//else
						cv::circle(imgFiltered, p2, 1, CV_RGB(0, 255, 255), -1);

						// Combined Intensity of the subpixel
						int pixelIntensity = subpixSampleSafe(grayScale, subPixel);

						// Converte from index to pixel coordinate
						// m (Column, real) -> -1,0,1 but we need to map to 0,1,2 -> add 1 to 0..2
						int w = m + 1;

						// n (Row, real) -> add stripeLenght >> 1 to shift to 0..stripeLength
						// n=0 -> -length/2, n=length/2 -> 0 ........ + length/2
						int h = n + (strip.stripeLength >> 1);

						// Set pointer to correct position and safe subpixel intensity
						imagePixelStripe.at<uchar>(h, w) = (uchar)pixelIntensity;
					}
				}

				// Apply sobel operator on stripe

				// ( -1 , -2, -1 )
				// (  0 ,  0,  0 )
				// (  1 ,  2,  1 )

				// The first and last row must be excluded from the sobel calculation because they have no top or bottom neighbors
				std::vector<double> sobelValues(strip.stripeLength - 2.);

				// To use the kernel we start with the second row (n) and stop before the last one
				for (int n = 1; n < (strip.stripeLength - 1); n++) {
					// Take the intensity value from the stripe 
					unsigned char* stripePtr = &(imagePixelStripe.at<uchar>(n - 1, 0));

					// Calculation of the gradient with the sobel for the first row
					double r1 = -stripePtr[0] - 2. * stripePtr[1] - stripePtr[2];

					// Go two lines for the third line of the sobel, step = size of the data type, here uchar
					stripePtr += 2 * imagePixelStripe.step;

					// Calculation of the gradient with the sobel for the third row
					double r3 = stripePtr[0] + 2. * stripePtr[1] + stripePtr[2];

					// Writing the result into our sobel value vector
					unsigned int ti = n - 1;
					sobelValues[ti] = r1 + r3;
				}

				double maxIntensity = -1;
				int maxIntensityIndex = 0;

				// Finding the max value (where has the most sharp change)
				for (int n = 0; n < strip.stripeLength - 2; ++n) {
					if (sobelValues[n] > maxIntensity) {
						maxIntensity = sobelValues[n];
						maxIntensityIndex = n;
					}
				}

				// f(x) slide 7 -> y0 .. y1 .. y2
				double y0, y1, y2;

				// Point before and after
				unsigned int max1 = maxIntensityIndex - 1, max2 = maxIntensityIndex + 1;

				// If the index is at the border we are out of the stripe, then we will take 0
				y0 = (maxIntensityIndex <= 0) ? 0 : sobelValues[max1];
				y1 = sobelValues[maxIntensityIndex];
				// If we are going out of the array of the sobel values
				y2 = (maxIntensityIndex >= strip.stripeLength - 3) ? 0 : sobelValues[max2];

				// Formula for calculating the x-coordinate of the vertex of a parabola, given 3 points with equal distances 
				// (xv means the x value of the vertex, d the distance between the points): 
				// xv = x1 + (d / 2) * (y2 - y0)/(2*y1 - y0 - y2)

				// Equation system
				// d = 1 because of the normalization and x1 will be added later
				double pos = (y2 - y0) / (4 * y1 - 2 * y0 - 2 * y2);

				// If the found pos is not a number -> there is no solution
				if (isnan(pos)) {
					continue;
				}

				// Exact point with subpixel accuracy
				cv::Point2d edgeCenter;

				// Back to Index positioning, Where is the edge (max gradient) in the picture?
				int maxIndexShift = maxIntensityIndex - (strip.stripeLength >> 1);

				// Shift the original edgepoint accordingly -> Is the pixel point at the top or bottom?
				edgeCenter.x = (double)p.x + (((double)maxIndexShift + pos) * strip.stripeVecY.x);
				edgeCenter.y = (double)p.y + (((double)maxIndexShift + pos) * strip.stripeVecY.y);

				// Highlight the subpixel with blue color
				cv::circle(imgFiltered, edgeCenter, 2, CV_RGB(0, 0, 255), -1);

				edgePointCenters[j - 1].x = edgeCenter.x;
				edgePointCenters[j - 1].y = edgeCenter.y;

				// Draw the stripe in the image
				//if (isFirstStripe) {
				//	cv::Mat iplTmp;
				//	// The intensity differences on the stripe
				//	cv::resize(imagePixelStripe, iplTmp, cv::Size(100, 300));

				//	cv::imshow("Strip", iplTmp);
				//	isFirstStripe = false;
				//}
			}

			// We now have the array of exact edge centers stored in "points"
			// Every row has two values -> 2 channels!
			cv::Mat highIntensityPoints(cv::Size(1, 6), CV_32FC2, edgePointCenters);

			// fitLine stores the calculated line in lineParams per column in the following way:
			// vec.x, vec.y, point.x, point.y
			// Norm 2, 0 and 0.01 -> Optimal parameters
			// i -> Edge points
			cv::fitLine(highIntensityPoints, lineParamsMat.col(i), cv::DIST_L2, 0, 0.01, 0.01);
			// We need two points to draw the line
			cv::Point p1;
			// We have to jump through the 4x4 cv::Matrix, meaning the next value for the wanted line is in the next row -> +4
			// d = -50 is the scalar -> Length of the line, g: Point + d*Vector
			// p1<----Middle---->p2
			//   <-----100----->
			p1.x = (int)lineParams[8 + i] - (int)(50.0 * lineParams[i]);
			p1.y = (int)lineParams[12 + i] - (int)(50.0 * lineParams[4 + i]);

			cv::Point p2;
			p2.x = (int)lineParams[8 + i] + (int)(50.0 * lineParams[i]);
			p2.y = (int)lineParams[12 + i] + (int)(50.0 * lineParams[4 + i]);

			// Draw line
			cv::line(imgFiltered, p1, p2, CV_RGB(0, 255, 255), 3, 8, 0);
		}

		// So far we stored the exact line parameters and show the lines in the image 
		// Now we have to calculate the exact corners
		cv::Point2f corners[4];
		std::vector<cv::Point2f> copyCorners(4);

		// Calculate the intersection points of both lines
		for (int i = 0; i < 4; ++i) {
			// Go through the corners of the rectangle, 3 -> 0
			int j = (i + 1) % 4;

			double x0, x1, y0, y1, u0, u1, v0, v1;

			// We have to jump through the 4x4 cv::Matrix
			// meaning the next value for the wanted line is in the next row -> +4
			// g: Point + d*Vector
			// g1 = (x0,y0) + scalar0*(u0,v0) == g2 = (x1,y1) + scalar1*(u1,v1)
			x0 = lineParams[i + 8]; y0 = lineParams[i + 12];
			x1 = lineParams[j + 8]; y1 = lineParams[j + 12];

			// Direction vector
			u0 = lineParams[i]; v0 = lineParams[i + 4];
			u1 = lineParams[j]; v1 = lineParams[j + 4];

			// (x|y) = p + s * vec --> Vector Equation

			// (x|y) = p + (Ds / D) * vec

			// p0.x = x0; p0.y = y0; vec0.x= u0; vec0.y=v0;
				// p0 + s0 * vec0 = p1 + s1 * vec1
				// p0-p1 = vec(-vec0 vec1) * vec(s0 s1)	

				// s0 = Ds0 / D (see cramer's rule)
				// s1 = Ds1 / D (see cramer's rule)   
				// Ds0 = -(x0-x1)v1 + (y0-y1)u1 --> You need to just calculate one, here Ds0

			// (x|y) = (p * D / D) + (Ds * vec / D)
			// (x|y) = (p * D + Ds * vec) / D

				// x0 * D + Ds0 * u0 / D    or   x1 * D + Ds1 * u1 / D     --> a / D
				// y0 * D + Ds0 * v0 / D    or   y1 * D + Ds1 * v1 / D     --> b / D						   		

			// (x|y) = a / c;

			// Cramer's rule
			// 2 unknown a,b -> Equation system
			double a = x1 * u0 * v1 - y1 * u0 * u1 - x0 * u1 * v0 + y0 * u0 * u1;
			double b = -x0 * v0 * v1 + y0 * u0 * v1 + x1 * v0 * v1 - y1 * v0 * u1;

			// Calculate the cross product to check if both direction vectors are parallel -> = 0
			// c -> Determinant = 0 -> linear dependent -> the direction vectors are parallel -> No division with 0
			double c = v1 * u0 - v0 * u1;
			if (fabs(c) < 0.001) {
				std::cout << "lines parallel" << std::endl;
				continue;
			}

			// We have checked for parallelism of the direction vectors
			// -> Cramer's rule, now divide through the main determinant
			a /= c;
			b /= c;

			// Exact corner
			corners[i].x = a;
			corners[i].y = b;
			copyCorners[i] = cv::Point2f(a, b);

			cv::Point p;
			p.x = (int)corners[i].x;
			p.y = (int)corners[i].y;

			cv::circle(imgFiltered, p, 5, CV_RGB(255, 255, 0), -1);
		} // End of the loop to extract the exact corners

		// Draw center of corners
		cv::Point2f center = getCenterOfCorners(copyCorners);
		cv::circle(imgFiltered, center, 5, CV_RGB(255, 0, 0), -1);

		// Coordinates on the original marker images to go to the actual center of the first pixel -> 100 * 100
		cv::Point2f targetCorners[4];
		targetCorners[0].x = -0.5; targetCorners[0].y = -0.5;
		targetCorners[1].x = 99.5; targetCorners[1].y = -0.5;
		targetCorners[2].x = 99.5; targetCorners[2].y = 99.5;
		targetCorners[3].x = -0.5; targetCorners[3].y = 99.5;

		// Create and calculate the cv::Matrix of perspective transform -> non-affine -> parallel stays not parallel
		// Homography is a cv::Matrix to describe the transforcv::Mation 
		// From an image region to the 2D projected image
		cv::Mat homographyMatrix(cv::Size(3, 3), CV_32FC1);
		// Corner which we calculated and our target cv::Mat, find the transforcv::Mation
		homographyMatrix = cv::getPerspectiveTransform(corners, targetCorners);

		// Create image for the marker
		cv::Mat imageMarker(cv::Size(100, 100), CV_8UC1);

		// Change the perspective in the marker image using the previously calculated Homography cv::Matrix
		// In the Homography cv::Matrix there is also the position in the image saved
		cv::warpPerspective(grayScale, imageMarker, homographyMatrix, cv::Size(100, 100));

		// Now we have a B/W image of a supposed Marker include Kanji
		cv::threshold(imageMarker, imageMarker, 55, 255, cv::THRESH_BINARY);
		// Use Erosion to expand the black glyph part, make the kanji solider
		int dilation_size = 1;
		cv::Mat element = getStructuringElement(cv::MORPH_RECT,
			cv::Size(2 * dilation_size + 1, 2 * dilation_size + 1),
			cv::Point(dilation_size, dilation_size));
		cv::Mat erodedMarker;
		cv::erode(imageMarker, erodedMarker, element);

		// Flood fill in four corners to remove the border 
		cv::floodFill(imageMarker, cv::Point(0, 0), cv::Scalar(255, 255, 255));
		cv::floodFill(imageMarker, cv::Point(0, imageMarker.rows - 1), cv::Scalar(255, 255, 255));
		cv::floodFill(imageMarker, cv::Point(imageMarker.cols - 1, 0), cv::Scalar(255, 255, 255));
		cv::floodFill(imageMarker, cv::Point(imageMarker.cols - 1, imageMarker.rows - 1), cv::Scalar(255, 255, 255));
		float floodMean = mean(imageMarker)[0];
		if (floodMean >= 240 || floodMean <= 128) {
			continue;
		}

		// Detect Left Bottom corner to detect whether the marker is correctly rotated
		cv::Rect myROI(0, 80, 20, 20);
		cv::Mat croppedArea = erodedMarker(myROI);

		int counter = 0;

		// If mean > 128 (not that black) => need rotation
		while (cv::mean(croppedArea)[0] > 128) {
			cv::rotate(erodedMarker, erodedMarker, cv::ROTATE_90_CLOCKWISE);
			counter++;
			croppedArea = erodedMarker(myROI);
			if (counter >= 4) {
				break;
			}
		}
		if (counter >= 4) {
			continue;
		}

		cv::imshow("ErodedMarker", erodedMarker);
		cv::floodFill(erodedMarker, cv::Point(0, 0), cv::Scalar(255, 255, 255));
		cv::floodFill(erodedMarker, cv::Point(0, erodedMarker.rows - 1), cv::Scalar(255, 255, 255));
		cv::floodFill(erodedMarker, cv::Point(erodedMarker.cols - 1, 0), cv::Scalar(255, 255, 255));
		cv::floodFill(erodedMarker, cv::Point(erodedMarker.cols - 1, erodedMarker.rows - 1), cv::Scalar(255, 255, 255));

		// Pass the eroded and floodfilled marker to Tesseract for Character Recognition
		api->SetImage(mat8ToPix(&erodedMarker));

		// Get Detected Text from Tesseract API
		std::string outText = std::string(api->GetUTF8Text());

		// Identifying which kanji is detected
		int foundIdx = -1;
		for (int i = 0; i < objs.size(); i++) {
			if (outText.find(objs[i]["kanji"].asString()) == std::string::npos) {
				continue;
			}
			foundIdx = i;
		}

		// If no kanji detected also no need to continue the following steps
		if (foundIdx == -1) {
			continue;
		} 

		// Save the Corners for found Kanji
		detectedMarkerCorners[objs[foundIdx]["id"].asInt()] = copyCorners;

		// Correct the order of the corners, if 0 -> already have the 0 degree position
		if (counter != 0) {
			cv::Point2f corrected_corners[4];
			// Smallest id represents the x-axis, we put the values in the corrected_corners array
			for (int i = 0; i < 4; i++)	corrected_corners[(counter + i) % 4] = corners[i];
			// We put the values back in the array in the sorted order
			for (int i = 0; i < 4; i++)	corners[i] = corrected_corners[i];
		}

		// Transfer screen coords to camera coords -> To get to the principal point
		for (int i = 0; i < 4; i++) {
			// Here you have to use your own camera resolution (x) * 0.5
			corners[i].x -= 320;
			// -(corners.y) -> is needed because y is inverted
			// Here you have to use your own camera resolution (y) * 0.5
			corners[i].y = -corners[i].y + 240;
		}

		// 4x4 -> Rotation | Translation
		//        0  0  0  | 1 -> (Homogene coordinates to combine rotation, translation and scaling)
		// 0.041 => Marker size in meters!
		estimateSquarePose(resultMatrix, (cv::Point2f*)corners, 0.041);

		// Change float[] to cv::Mat
		cv::Mat resPose = (cv::Mat_<float>(4, 4) << resultMatrix[0], resultMatrix[1], resultMatrix[2], resultMatrix[3],
													resultMatrix[4], resultMatrix[5], resultMatrix[6], resultMatrix[7],
													resultMatrix[8], resultMatrix[9], resultMatrix[10], resultMatrix[11],
													resultMatrix[12], resultMatrix[13], resultMatrix[14], resultMatrix[15]);
		
		// Save found Pose for detected Kanji
		detectedMarkers[objs[foundIdx]["id"].asInt()] = resPose;
	}

	//imshow("OpenCV", imgFiltered);
	//isFirstStripe = true;
	return imgFiltered;
}


std::map<int, std::vector<cv::Point2f>> Tracker::getDetectedMarkerCorners() {
	return detectedMarkerCorners;
}

void Tracker::cleanDetectedMarkers() {
	detectedMarkers.clear();
	detectedMarkerCorners.clear();
	//detectedMarkerRotated.clear();
}

std::map<int, cv::Point2f> Tracker::getDetectedMarkerCenter() {
	std::map<int, cv::Point2f> res;

	for (auto const& x : detectedMarkerCorners)
	{
		cv::Point2f calculatedCenter = getCenterOfCorners(x.second);
		res[x.first] = calculatedCenter;
	}

	return res;
}

std::map<int, cv::Mat> Tracker::getDetectedMarkerPose() {
	return detectedMarkers;
}

cv::Mat Tracker::getMarkerPoseById(int id) {
	return detectedMarkers.at(id);
}

cv::Point2f Tracker::getMarkerCenterById(int id) {
	return getCenterOfCorners(detectedMarkerCorners.at(id));
}

std::vector<cv::Point2f> Tracker::getMarkerCornersById(int id) {
	return detectedMarkerCorners.at(id);
}
