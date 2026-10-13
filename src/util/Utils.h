#pragma once

#include <cstdint>

class CUtils {
public:
    CUtils() {}
    virtual ~CUtils() {}

    static int32_t random(int32_t seed);
    static double urandom(double seed);
};

extern CUtils g_utils;
