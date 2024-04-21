#include "concentricRingCalibrator.h"

namespace slmaster {
namespace calibration {

ConcentricRingCalibrator::ConcentricRingCalibrator() {}

ConcentricRingCalibrator::~ConcentricRingCalibrator() {}

bool ConcentricRingCalibrator::findFeaturePoints(
    const cv::Mat &img, const cv::Size &featureNums,
    std::vector<cv::Point2f> &points, const ThreshodMethod threshodType) {
    CV_Assert(!img.empty());
    points.clear();

    return findConcentricRingGrid(img, featureNums, radius_, points);
}

double ConcentricRingCalibrator::calibrate(const std::vector<cv::Mat> &imgs,
                                           cv::Mat &intrinsic, cv::Mat &distort,
                                           const cv::Size &featureNums,
                                           float &process,
                                           const ThreshodMethod threshodType,
                                           const bool blobBlack) {
    imgPoints_.clear();
    worldPoints_.clear();

    std::vector<cv::Point3f> worldPointsCell;
    for (int i = 0; i < featureNums.height; ++i) {
        for (int j = 0; j < featureNums.width; ++j) {
            worldPointsCell.emplace_back(
                cv::Point3f(j * distance_, i * distance_, 0));
        }
    }
    for (int i = 0; i < imgs.size(); ++i)
        worldPoints_.emplace_back(worldPointsCell);

    for (int i = 0; i < imgs.size(); ++i) {
        std::vector<cv::Point2f> imgPointCell;
        if (!findFeaturePoints(imgs[i], featureNums, imgPointCell)) {
            return i;
        } else {
            imgPoints_.emplace_back(imgPointCell);

            cv::Mat imgWithFeature = imgs[i].clone();
            if (imgWithFeature.type() == CV_8UC1) {
                cv::cvtColor(imgWithFeature, imgWithFeature,
                             cv::COLOR_GRAY2BGR);
            }

            cv::drawChessboardCorners(imgWithFeature, featureNums, imgPointCell,
                                      true);
            drawedFeaturesImgs_.push_back(imgWithFeature);

            process = static_cast<float>(i + 1) / imgs.size();
        }
    }

    std::vector<cv::Mat> rvecs, tvecs;
    double error = cv::calibrateCamera(worldPoints_, imgPoints_, imgs[0].size(),
                                       intrinsic, distort, rvecs, tvecs);

    for (int i = 0; i < worldPoints_.size(); ++i) {
        std::vector<cv::Point2f> reprojectPoints;
        std::vector<cv::Point2f> curErrorsDistribute;
        cv::projectPoints(worldPoints_[i], rvecs[i], tvecs[i], intrinsic,
                          distort, reprojectPoints);
        for (int j = 0; j < reprojectPoints.size(); ++j) {
            curErrorsDistribute.emplace_back(
                cv::Point2f(reprojectPoints[j].x - imgPoints_[i][j].x,
                            reprojectPoints[j].y - imgPoints_[i][j].y));
        }
        errors_.emplace_back(curErrorsDistribute);
    }

    return error;
}

void ConcentricRingCalibrator::sortElipse(
    const std::vector<cv::RotatedRect> &rects,
    const std::vector<std::vector<cv::Point2f>> &rectsPoints,
    const std::vector<cv::Point2f> &sortedCenters,
    std::vector<std::vector<cv::RotatedRect>> &sortedRects) {
    sortedRects.clear();

    auto diffNearestPoint = sortedCenters[0] - sortedCenters[1];
    const float distanceFeat =
        std::sqrtf(diffNearestPoint.x * diffNearestPoint.x +
                   diffNearestPoint.y * diffNearestPoint.y) /
        2.f;

    for (size_t i = 0; i < sortedCenters.size(); ++i) {
        std::vector<cv::RotatedRect> rectCurCluster;
        std::vector<std::vector<cv::Point2f>> rectPointsCurCluster;
        for (size_t j = 0; j < rects.size(); ++j) {
            cv::Point2f dist = rects[j].center - sortedCenters[i];
            float distance = std::sqrt(dist.x * dist.x + dist.y * dist.y);

            if (distance < distanceFeat) {
                bool isNeedMerged = false;
                for (size_t d = 0; d < rectCurCluster.size(); ++d) {
                    if (std::abs(rects[j].size.width -
                                 rectCurCluster[d].size.width) < 5.f) {
                        std::vector<cv::Point2f> mergePoints;
                        mergePoints.insert(mergePoints.end(),
                                           rectsPoints[j].begin(),
                                           rectsPoints[j].end());
                        mergePoints.insert(mergePoints.end(),
                                           rectPointsCurCluster[d].begin(),
                                           rectPointsCurCluster[d].end());
                        cv::RotatedRect ellipseFited =
                            cv::fitEllipse(mergePoints);
                        rectCurCluster[d] = ellipseFited;
                        rectPointsCurCluster[d] = mergePoints;

                        isNeedMerged = true;
                        break;
                    }
                }

                if (!isNeedMerged) {
                    rectCurCluster.emplace_back(rects[j]);
                    rectPointsCurCluster.emplace_back(rectsPoints[j]);
                }
            }
        }
        /*
        for (int i = 0; i < rectPointsCurCluster.size(); ++i) {
            static int curIndex = 0;
            cv::FileStorage writeFile("circle_" + std::to_string(curIndex++) +
                                          ".yml",
                                      cv::FileStorage::WRITE);
            for (int j = 0; j < rectPointsCurCluster[i].size(); ++j) {
                cv::Mat pt =
                    (cv::Mat_<float>(2, 1) << rectPointsCurCluster[i][j].x,
                     rectPointsCurCluster[i][j].y);
                writeFile << "pt_" + std::to_string(j) << pt;
            }
        }
        */
        std::sort(rectCurCluster.begin(), rectCurCluster.end(),
                  [](cv::RotatedRect &lhs, cv::RotatedRect &rhs) -> bool {
                      return lhs.size.area() < rhs.size.area();
                  });

        sortedRects.emplace_back(rectCurCluster);
    }
}

bool ConcentricRingCalibrator::findConcentricRingGrid(
    const cv::Mat &inputImg, const cv::Size patternSize,
    const std::vector<float> radius, std::vector<cv::Point2f> &centerPoints) {

    std::vector<cv::Point2f> ellipseCenter;
    cv::Mat threshodFindCircle = inputImg.clone();

    if (inputImg.type() == CV_8UC3) {
        cv::cvtColor(inputImg, threshodFindCircle, cv::COLOR_BGR2GRAY);
    }

    cv::adaptiveThreshold(threshodFindCircle, threshodFindCircle, 255,
                          cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 61, 0);

    cv::SimpleBlobDetector::Params params;
    params.blobColor = 255;
    params.filterByCircularity = true;
    cv::Ptr<cv::SimpleBlobDetector> detector =
        cv::SimpleBlobDetector::create(params);

    std::vector<cv::Point2f> pointsOfCell;
    bool isFind =
        cv::findCirclesGrid(threshodFindCircle, patternSize, pointsOfCell,
                            cv::CALIB_CB_SYMMETRIC_GRID, detector);
    if (!isFind) {
        pointsOfCell.clear();
        isFind = cv::findCirclesGrid(
            threshodFindCircle, cv::Size(patternSize.width, patternSize.height),
            pointsOfCell, cv::CALIB_CB_SYMMETRIC_GRID | cv::CALIB_CB_CLUSTERING,
            detector);
    }

    if (!isFind) {
        return false;
    }

    auto diffNearestPoint = pointsOfCell[0] - pointsOfCell[1];
    const float distanceFeat =
        std::sqrtf(diffNearestPoint.x * diffNearestPoint.x +
                   diffNearestPoint.y * diffNearestPoint.y) /
        2.f;

    std::vector<cv::Vec4i> hierarchy;
    std::vector<cv::RotatedRect> ellipses;
    std::vector<std::vector<cv::Point2f>> ellipsesPoints;
    std::vector<Contour> contours;
    std::vector<std::vector<cv::Point2i>> contours2Draw;

    cv::Mat inputImgClone = inputImg.clone();
    if (inputImg.type() == CV_8UC3) {
        cv::cvtColor(inputImg, inputImgClone, cv::COLOR_BGR2GRAY);
    }

    EdgesSubPix(inputImgClone, 1.5, 20, 40, contours, hierarchy, cv::RETR_CCOMP);
    for (int i = 0; i < contours.size(); i++) {
        if (contours[i].points.size() < 20 ||
            contours[i].points.size() > 1000) {
            continue;
        }

        std::vector<cv::Point2f> polyContour;
        cv::approxPolyDP(contours[i].points, polyContour, 0.01, true);
        float area = cv::contourArea(polyContour, false);
        float length = cv::arcLength(polyContour, false);
        float circleRatio = 4.0 * CV_PI * area / (length * length);
        if (circleRatio < 0.6f) {
            continue;
        }

        cv::RotatedRect ellipseFited = cv::fitEllipse(contours[i].points);

        bool isEfficient = false;
        for (size_t j = 0; j < pointsOfCell.size(); ++j) {
            cv::Point dist = pointsOfCell[j] - ellipseFited.center;
            float distance = std::sqrt(dist.x * dist.x + dist.y * dist.y);
            if (distance < distanceFeat) {
                isEfficient = true;
                break;
            }
        }

        if (!isEfficient) {
            continue;
        }

        ellipses.emplace_back(ellipseFited);
        ellipsesPoints.emplace_back(contours[i].points);
    }

    std::vector<std::vector<cv::RotatedRect>> sortedRects;
    sortElipse(ellipses, ellipsesPoints, pointsOfCell, sortedRects);
    /*
    cv::Mat testMat = threshodFindCircle.clone();
    cv::cvtColor(testMat, testMat, cv::COLOR_GRAY2BGR);
    for (int j = 0; j < sortedRects.size(); ++j) {
        for (int k = 0; k < sortedRects[j].size(); ++k) {
            cv::ellipse(testMat, sortedRects[j][k], cv::Scalar(0, 255, 0));
        }
    }

    cv::drawChessboardCorners(testMat,
                              cv::Size(patternSize.width, patternSize.height),
                              pointsOfCell, true);
    */
    if (sortedRects.size() != patternSize.width * patternSize.height) {
        return false;
    }

    std::vector<std::vector<cv::Mat>> circleNormalMat;
    for (int i = 0; i < sortedRects.size(); i++) {
        if (sortedRects[i].size() < 4) {
            return false;
        }

        std::vector<cv::Mat> normalMatsCluster(4);
        getEllipseNormalQuation(sortedRects[i][0], normalMatsCluster[0]);
        getEllipseNormalQuation(sortedRects[i][1], normalMatsCluster[1]);
        getEllipseNormalQuation(sortedRects[i][2], normalMatsCluster[2]);
        getEllipseNormalQuation(sortedRects[i][3], normalMatsCluster[3]);
        circleNormalMat.emplace_back(normalMatsCluster);
    }

    getRingCenters(circleNormalMat, radius, centerPoints);

    return true;
}

void ConcentricRingCalibrator::getRingCenters(
    const std::vector<std::vector<cv::Mat>> &normalMats,
    const std::vector<float> &radius, std::vector<cv::Point2f> &points) {
    points.clear();

    cv::Mat quationToSolve;
    Eigen::Matrix<float, 3, 3> polarCircleMat;

    for (int d = 0; d < normalMats.size(); ++d) {
        std::vector<cv::Mat> matchEllipse = {normalMats[d][0], normalMats[d][1],
                                             normalMats[d][2],
                                             normalMats[d][3]};
        float minimunEignValueDistance = FLT_MAX;
        float eignValueDistance = FLT_MAX;
        cv::Point2f center;

        for (int i = 3; i > 1; i--) {
            for (int j = i - 1; j > 0; j--) {
                quationToSolve = matchEllipse[i].inv() * matchEllipse[j];
                polarCircleMat << quationToSolve.at<float>(0, 0),
                    quationToSolve.at<float>(0, 1),
                    quationToSolve.at<float>(0, 2),
                    quationToSolve.at<float>(1, 0),
                    quationToSolve.at<float>(1, 1),
                    quationToSolve.at<float>(1, 2),
                    quationToSolve.at<float>(2, 0),
                    quationToSolve.at<float>(2, 1),
                    quationToSolve.at<float>(2, 2);
                Eigen::EigenSolver<Eigen::Matrix3f> eignSolver(polarCircleMat);
                Eigen::Matrix3f value = eignSolver.pseudoEigenvalueMatrix();
                value = value * std::pow(radius[j] / radius[i], 2);
                Eigen::Matrix3f vector = eignSolver.pseudoEigenvectors();

                if (std::abs(value(0, 0) - value(1, 1)) < 0.2f) {
                    eignValueDistance = std::pow(value(0, 0) - 1.f, 2) +
                                        std::pow(value(1, 1) - 1.f, 2);

                    if (eignValueDistance < minimunEignValueDistance) {
                        minimunEignValueDistance = eignValueDistance;
                        center.x = vector(0, 2) / vector(2, 2);
                        center.y = vector(1, 2) / vector(2, 2);
                    }
                } else if (std::abs(value(0, 0) - value(2, 2)) < 0.2f) {
                    eignValueDistance = std::pow(value(0, 0) - 1.f, 2) +
                                        std::pow(value(2, 2) - 1.f, 2);

                    if (eignValueDistance < minimunEignValueDistance) {
                        minimunEignValueDistance = eignValueDistance;
                        center.x = vector(0, 1) / vector(2, 1);
                        center.y = vector(1, 1) / vector(2, 1);
                    }
                } else {
                    eignValueDistance = std::pow(value(1, 1) - 1.f, 2) +
                                        std::pow(value(2, 2) - 1.f, 2);

                    if (eignValueDistance < minimunEignValueDistance) {
                        minimunEignValueDistance = eignValueDistance;
                        center.x = vector(0, 0) / vector(2, 0);
                        center.y = vector(1, 0) / vector(2, 0);
                    }
                }
            }
        }
        
        points.push_back(center);
    }
}

void ConcentricRingCalibrator::getEllipseNormalQuation(
    const cv::RotatedRect &rotateRect, cv::Mat &quationMat) {

    float angle = rotateRect.angle * CV_PI / 180.0;
    float a = rotateRect.size.height / 2.f;
    float b = rotateRect.size.width / 2.f;
    float xc = rotateRect.center.x;
    float yc = rotateRect.center.y;

    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    float A = cosAngle * cosAngle / (a * a) + sinAngle * sinAngle / (b * b);
    float B = 2 * cosAngle * sinAngle * (1 / (a * a) - 1 / (b * b));
    float C = sinAngle * sinAngle / (a * a) + cosAngle * cosAngle / (b * b);
    float D = -2 * A * xc - B * yc;
    float E = -2 * C * yc - B * xc;
    float F = A * xc * xc + B * xc * yc + C * yc * yc - 1;

    cv::Mat normalMat(3, 3, CV_32F, cv::Scalar(0.f));
    normalMat.at<float>(0, 0) = A;
    normalMat.at<float>(0, 1) = B / 2.f;
    normalMat.at<float>(0, 2) = D / 2.f;
    normalMat.at<float>(1, 0) = normalMat.at<float>(0, 1);
    normalMat.at<float>(1, 1) = C;
    normalMat.at<float>(1, 2) = E / 2.f;
    normalMat.at<float>(2, 0) = normalMat.at<float>(0, 2);
    normalMat.at<float>(2, 1) = normalMat.at<float>(1, 2);
    normalMat.at<float>(2, 2) = F;

    normalMat.copyTo(quationMat);
}

bool ConcentricRingCalibrator::sortKeyPoints(
    const std::vector<cv::Point2f> &inputPoints, const cv::Size patternSize,
    std::vector<cv::Point2f> &outputPoints) {
    cv::RotatedRect rect = cv::minAreaRect(inputPoints);
    cv::Point2f rectTopPoints[4];
    rect.points(rectTopPoints);
    cv::Point2f leftUpper;
    cv::Point2f rightUpper;
    std::vector<cv::Point2f> comparePoint;
    std::vector<int> indexPoint;
    int index_LeftUp;
    std::vector<float> x_Rect = {rectTopPoints[0].x, rectTopPoints[1].x,
                                 rectTopPoints[2].x, rectTopPoints[3].x};
    std::sort(x_Rect.begin(), x_Rect.end());
    for (int i = 0; i < 4; i++) {
        if (rectTopPoints[i].x == x_Rect[0] ||
            rectTopPoints[i].x == x_Rect[1]) {
            comparePoint.push_back(rectTopPoints[i]);
            indexPoint.push_back(i);
        }
    }
    if (comparePoint[0].y < comparePoint[1].y) {
        leftUpper = comparePoint[0];
        index_LeftUp = indexPoint[0];
    } else {
        leftUpper = comparePoint[1];
        index_LeftUp = indexPoint[1];
    }
    if (index_LeftUp == 0) {
        float value_first = std::pow((leftUpper.x - rectTopPoints[1].x), 2) +
                            std::pow((leftUpper.y - rectTopPoints[1].y), 2);
        float value_second = std::pow((leftUpper.x - rectTopPoints[3].x), 2) +
                             std::pow((leftUpper.y - rectTopPoints[3].y), 2);
        if (value_first < value_second) {
            rightUpper = rectTopPoints[3];
        } else {
            rightUpper = rectTopPoints[1];
        }
    } else {
        float value_first =
            std::pow((leftUpper.x - rectTopPoints[(index_LeftUp + 1) % 4].x),
                     2) +
            std::pow((leftUpper.y - rectTopPoints[(index_LeftUp + 1) % 4].y),
                     2);
        float value_second =
            std::pow((leftUpper.x - rectTopPoints[index_LeftUp - 1].x), 2) +
            std::pow((leftUpper.y - rectTopPoints[3].y), 2);
        if (value_first < value_second) {
            rightUpper = rectTopPoints[index_LeftUp - 1];
        } else {
            rightUpper = rectTopPoints[(index_LeftUp + 1) % 4];
        }
    }
    std::vector<float> distance;
    std::vector<float> distance_Sort;
    std::vector<cv::Point2f> sortDistancePoints;
    for (int i = 0; i < inputPoints.size(); i++) {
        float distanceValue =
            getDist_P2L(inputPoints[i], leftUpper, rightUpper);
        distance.push_back(distanceValue);
        distance_Sort.push_back(distanceValue);
    }
    std::sort(distance_Sort.begin(), distance_Sort.end());
    for (int i = 0; i < inputPoints.size(); i++) {
        for (int j = 0; j < inputPoints.size(); j++) {
            if (distance[j] == distance_Sort[i]) {
                sortDistancePoints.push_back(inputPoints[j]);
                break;
            }
        }
    }
    std::vector<float> valueX;
    int match;
    int rows = patternSize.height;
    int cols = patternSize.width;
    for (int i = 0; i < rows; i++) {
        valueX.clear();
        for (int j = 0; j < cols; j++) {
            valueX.push_back(sortDistancePoints[i * cols + j].x);
        }
        std::sort(valueX.begin(), valueX.end());
        for (int j = 0; j < valueX.size(); j++) {
            for (int k = 0; k < cols; k++) {
                if (valueX[j] == sortDistancePoints[i * cols + k].x) {
                    match = i * cols + k;
                    break;
                }
            }
            outputPoints.push_back(sortDistancePoints[match]);
        }
    }

    return true;
}

float ConcentricRingCalibrator::getDist_P2L(cv::Point2f pointP,
                                            cv::Point2f pointA,
                                            cv::Point2f pointB) {
    float A = 0, B = 0, C = 0;
    A = pointA.y - pointB.y;
    B = pointB.x - pointA.x;
    C = pointA.x * pointB.y - pointA.y * pointB.x;

    float distance = 0;
    distance = ((float)abs(A * pointP.x + B * pointP.y + C)) /
               ((float)sqrtf(A * A + B * B));

    return distance;
}

} // namespace calibration
} // namespace slmaster