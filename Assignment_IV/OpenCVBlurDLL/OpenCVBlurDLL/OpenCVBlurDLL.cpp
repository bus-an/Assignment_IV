// COpenCVBlurDLL.cpp : DLL의 초기화 루틴을 정의합니다.
//

#include "pch.h"
#include "framework.h"
#include "OpenCVBlurDLL.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BOOL ImageBlur(const ImageObject* src, ImageObject* dst, const int nKernelSize)
{
	BOOL bRet = TRUE;
	cv::Mat srcMat, dstMat;

	// dst 이미지 크기 선언 ///////////////////////////////////////////////////////////////
	dst->ch = src->ch;
	dst->cols = src->cols;
	dst->rows = src->rows;

	size_t totalSize = static_cast<size_t>(dst->rows) * dst->cols * dst->ch;
	//dst->data = new unsigned char[totalSize];
	memset(dst->data, 0xFF, totalSize);
	//////////////////////////////////////////////////////////////////////////////////////

	bRet &= CopyImageObjectToMat(src, srcMat);

	cv::Size kernelSize(nKernelSize, nKernelSize);
	cv::blur(srcMat, dstMat, kernelSize, cv::Point(-1, -1), cv::BORDER_DEFAULT);

	bRet &= CopyMatToImageObject(dstMat, dst);

	return bRet;
}

// ImageObject -> Mat
BOOL CopyImageObjectToMat(const ImageObject* obj, cv::Mat& mat)
{
	if (obj == nullptr || obj->data == nullptr || obj->rows <= 0 || obj->cols <= 0)
		return FALSE;

	mat.create(obj->rows, obj->cols, CV_8UC(obj->ch));

	size_t dataSize = static_cast<size_t>(obj->rows) * obj->cols * obj->ch;
	memcpy(mat.data, obj->data, dataSize);

	return TRUE;
}

// Mat -> ImageObject
BOOL CopyMatToImageObject(const cv::Mat& mat, ImageObject* obj)
{
	if (mat.empty() || obj == nullptr)
		return FALSE;

	obj->cols = mat.cols;
	obj->rows = mat.rows;
	obj->ch = mat.channels();

	// 전체 바이트 수 계산
	size_t dataSize = mat.total() * mat.elemSize();
	memcpy(obj->data, mat.data, dataSize);

	return TRUE;
}

