#pragma once

#ifdef IMAGEOBJECT_EXPORTS
#define IMAGEOBJECT_API __declspec(dllexport)
#else
#define IMAGEOBJECT_API __declspec(dllimport)
#endif

#include <cstring>

struct IMAGEOBJECT_API ImageObject
{
    int ch;                     // 이미지 채널
    int cols;                   // 이미지 가로
    int rows;                   // 이미지 세로
    unsigned char* data;        // 버퍼값

    ImageObject()
        : ch(-1), cols(-1), rows(-1), data(nullptr)
    {
        
    }
};
