/**
 * @file Corpse.h
 * @brief 尸体类，管理尸体创建、衰变与心跳。
 */

#pragma once

#include "Char.h"

/**
 * @brief 尸体对象，角色死亡后生成，随时间衰变消失。
 */
class CCorpse : public CChar {
public:
    static CCorpse* make_corpse(CChar* victim, CChar* killer);
    virtual void heart_beat();
    virtual int32_t query_attr(int attr);
    CCorpse();
    virtual ~CCorpse();

protected:
    void decay();

    int m_decayed;
    int m_delay;
};
