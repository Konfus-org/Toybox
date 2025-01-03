#pragma once
#include "TbxAPI.h"
#include "TbxPCH.h"

struct TBX_API Matrix
{
public:
    Matrix() = default;
    explicit(false) Matrix(const std::array<float, 16>& data) : Data(data) {}

    std::array<float, 16> Data;

    static Matrix Identity() { return Matrix({ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }); }
    
    Matrix operator*(const Matrix& other) const
    {
        Matrix result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.Data[i * 4 + j] = Data[i * 4] * other.Data[j] + Data[i * 4 + 1] * other.Data[j + 4] + Data[i * 4 + 2] * other.Data[j + 8] + Data[i * 4 + 3] * other.Data[j + 12];
            }
        }
        return result;
    }
};
