#pragma once

#ifdef OPENCVBLUR_EXPORTS
#define OPENCVBLUR_API __declspec(dllexport)
#else
#define OPENCVBLUR_API __declspec(dllimport)
#endif

#include <string>
#include <iostream>

// opencv
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

#include "ImageObject.h"

extern "C" BOOL AFX_EXT_CLASS ImageBlur(const ImageObject* src, ImageObject* dst, const int kernelSize);

BOOL CopyImageObjectToMat(const ImageObject* obj, cv::Mat& mat);

BOOL CopyMatToImageObject(const cv::Mat& mat, ImageObject* obj);
