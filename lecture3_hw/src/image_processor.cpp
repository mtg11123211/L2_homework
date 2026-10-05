// Provided infrastructure. You do not need to modify this file.

#include "image_processor.hpp"

#include <string>
#include <opencv2/imgproc.hpp>

cv::Mat ImageProcessor::process(const Frame &frame) const
{
    cv::Mat output;
    cv::resize(frame.image, output, cv::Size(320, 240));
    cv::rectangle(output, cv::Rect(30, 45, 120, 60), cv::Scalar(0, 255, 0), 2);
    cv::putText(output,
                "frame " + std::to_string(frame.id),
                cv::Point(20, 28),
                cv::FONT_HERSHEY_SIMPLEX,
                0.65,
                cv::Scalar(255, 255, 255),
                1,
                cv::LINE_AA);
    return output;
}
