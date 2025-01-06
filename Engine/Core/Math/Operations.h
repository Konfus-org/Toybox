#pragma once

#define BIT(x) (1 << (x))

namespace Tbx::Math
{
    static float Cos(float x)
    {
        // This implementation uses a Taylor series expansion up to 10 terms,
        // which should provide a reasonable approximation for most cases.
        float result = 0.0f;
        float term = x;
        float sign = 1.0f;
        int i = 3;
        while (i <= 10)
        {
            result += term * sign;
            term *= (x * x) / (float)(i * (i - 1));
            sign *= -1.0f;
            i += 2;
        }
        return result;
    }

    static float Sin(float x)
    {
        // This implementation uses a Taylor series expansion up to 10 terms, 
        // which should provide a reasonable approximation for most cases.
        float result = 1.0f;
        float term = 1.0f;
        int n = 0;
        while (n < 10)
        {
            term *= (x * x) / ((float)(2 * n + 1) * (float)(2 * n + 2));
            result += term;
            n++;
        }
        return result;
    }
}
