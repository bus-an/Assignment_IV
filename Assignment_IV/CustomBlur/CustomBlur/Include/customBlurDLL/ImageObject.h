#pragma once

#ifdef IMAGEOBJECT_EXPORTS
#define IMAGEOBJECT_API __declspec(dllexport)
#else
#define IMAGEOBJECT_API __declspec(dllimport)
#endif

#include <cstring>

struct IMAGEOBJECT_API ImageObject
{
    int ch;                     // �̹��� ä��
    int cols;                   // �̹��� ����
    int rows;                   // �̹��� ����
    unsigned char* data;        // ���۰�

    ImageObject()
        : ch(-1), cols(-1), rows(-1), data(nullptr)
    {
        
    }
};
