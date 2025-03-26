#pragma once

#ifdef CUSTOMBLUR_EXPORTS
#define CUSTOMBLUR_API __declspec(dllexport)
#else
#define CUSTOMBLUR_API __declspec(dllimport)
#endif

#include <string>
#include <iostream>

#include "ImageObject.h"

extern "C" BOOL AFX_EXT_CLASS ImageBlur(const ImageObject* src, ImageObject* dst, const int kernelSize);