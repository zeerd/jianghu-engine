#pragma once

#include <cstdint>

#include "../config/MainConfig.h"

class CUtils {
public:
    CUtils() {}
    virtual ~CUtils() {}

    // 上界取自槽位（读出来是槽位宽），返回值域是 [0, seed)——与上界同宽，
    // 因此入参与出参都随 mapping_value_t，不收窄。
    static mapping_value_t random(mapping_value_t seed);
    static double urandom(double seed);
};

extern CUtils g_utils;
