// CCustomBlurDLL.cpp : DLL의 초기화 루틴을 정의합니다.
//

#define NOMINMAX

#include "pch.h"
#include "framework.h"
#include "CustomBlurDLL.h"

#include <Windows.h>
#include <algorithm>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BOOL ImageBlur(const ImageObject * src, ImageObject * dst, const int kernelSize)
{
    // 기본 검사: src와 dst, 그리고 src->data, dst->data가 유효한지, 그리고 크기가 0보다 큰지 확인
    if (!src || !dst || !src->data)
        return FALSE;
    if (src->cols <= 0 || src->rows <= 0 || kernelSize <= 0)
        return FALSE;
    if (src->ch != 1)  // 단일 채널만 지원
        return FALSE;

    int width = src->cols;
    int height = src->rows;
    int anchor = kernelSize / 2; // 기본 anchor: 커널의 중앙

    // 패딩 계산 (BORDER_REFLECT_101 방식)
    int pad_top = anchor;
    int pad_left = anchor;
    int pad_bottom = kernelSize - anchor - 1;
    int pad_right = kernelSize - anchor - 1;
    int paddedWidth = width + pad_left + pad_right;
    int paddedHeight = height + pad_top + pad_bottom;

    // 패딩된 이미지를 저장할 버퍼 할당 (8-bit 단일 채널)
    unsigned char* padded = new unsigned char[paddedWidth * paddedHeight];
    if (!padded)
        return FALSE;

    // 패딩된 이미지를 BORDER_REFLECT_101 방식으로 채움
    // 각 좌표 (i, j) in padded 이미지에 대해, 대응하는 src 좌표를 계산
    for (int i = 0; i < paddedHeight; i++)
    {
        // 원본 좌표 y = i - pad_top (패딩이 적용되어 음수가 될 수 있음)
        int src_y = i - pad_top;
        // BORDER_REFLECT_101: if (src_y < 0) then src_y = -src_y; if (src_y >= height) then src_y = 2*height - src_y - 2;
        if (src_y < 0)
            src_y = -src_y;
        else if (src_y >= height)
            src_y = 2 * height - src_y - 2;

        for (int j = 0; j < paddedWidth; j++)
        {
            int src_x = j - pad_left;
            if (src_x < 0)
                src_x = -src_x;
            else if (src_x >= width)
                src_x = 2 * width - src_x - 2;

            padded[i * paddedWidth + j] = src->data[src_y * width + src_x];
        }
    }

    // 적분 이미지(Integral Image) 계산
    // 적분 이미지는 패딩된 이미지의 각 픽셀까지의 누적합을 저장합니다.
    // 여기서는 64비트 정수 배열을 사용합니다.
    int intHeight = paddedHeight + 1;
    int intWidth = paddedWidth + 1;
    // integral 배열의 크기는 (paddedHeight+1) x (paddedWidth+1)
    long long* integral = new long long[intHeight * intWidth];
    if (!integral)
    {
        delete[] padded;
        return FALSE;
    }
    // 첫 행과 첫 열은 0으로 초기화
    for (int i = 0; i < intHeight * intWidth; i++)
        integral[i] = 0;

    // 적분 이미지 계산: integral[i+1][j+1] = padded[i][j] + integral[i][j+1] + integral[i+1][j] - integral[i][j]
    for (int i = 0; i < paddedHeight; i++)
    {
        long long rowSum = 0;
        for (int j = 0; j < paddedWidth; j++)
        {
            rowSum += padded[i * paddedWidth + j];
            integral[(i + 1) * intWidth + (j + 1)] = integral[i * intWidth + (j + 1)] + rowSum;
        }
    }

    // 이제, 각 출력 픽셀 (y, x) (원본 이미지)에서, 대응하는 패딩된 이미지의 영역은:
    // top-left: (y, x) in padded, bottom-right: (y + kernelSize - 1, x + kernelSize - 1)
    // (왜냐하면 원본 (0,0) 픽셀은 padded의 (pad_top, pad_left) 위치에 있지만, integral 이미지에서는 인덱스 차이가 있으므로,
    // 출력 픽셀 (y,x) corresponds to padded region starting at (y, x) when padded is viewed from 0,0 because we already reflected)
    // 실제 계산: let r1 = y, c1 = x, r2 = y + kernelSize - 1, c2 = x + kernelSize - 1.
    // integral image 인덱스는 +1 offset.
    int area = kernelSize * kernelSize;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // padded 좌표에서 해당 출력 픽셀의 윗부분은 (y, x) offset을 보정하기 위해,
            // 실제 윗좌표 = y, x (왜냐하면 padded 이미지의 상단 패딩은 anchor, 그리고 y=0 corresponds to padded row = pad_top, 
            // but integral image is computed over full padded, so we use offset: r1 = y, because padded index (0,0) corresponds to
            // pixel that came from reflection for output ( -anchor ) ...  )
            // 보다 간단하게, observe:
            // 우리는 패딩 이미지를 전체 계산했고, 원본 (y,x)에서 원하는 커널 영역은 padded[ y : y+kernelSize-1, x : x+kernelSize-1 ] if we align
            // padded image with the top-left corner of the output as (0,0). 
            // 그러므로, 출력 픽셀 (y,x)의 영역 in padded image: r1 = y, c1 = x, r2 = y + kernelSize - 1, c2 = x + kernelSize - 1.
            // integral indices: R1 = r1, C1 = c1, R2 = r2+1, C2 = c2+1.
            int r1 = y;
            int c1 = x;
            int r2 = y + kernelSize - 1;
            int c2 = x + kernelSize - 1;
            // 경계 검사는 padded 이미지가 충분히 커야 함 (이미 계산됨)
            long long sum = integral[(r2 + 1) * intWidth + (c2 + 1)]
                - integral[r1 * intWidth + (c2 + 1)]
                - integral[(r2 + 1) * intWidth + c1]
                + integral[r1 * intWidth + c1];
            int avg = static_cast<int>((sum + area / 2) / area); // 반올림
            avg = min(255, max(0, avg));
            dst->data[y * width + x] = static_cast<unsigned char>(avg);
        }
    }

    // 할당한 메모리 해제
    delete[] padded;
    delete[] integral;

    return TRUE;
}
