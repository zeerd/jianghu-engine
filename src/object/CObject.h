/**
 * @file CObject.h
 * @brief 游戏对象基类，提供标识、属性与名称管理。
 */

#pragma once

#include "MainConfig.h"

#include "Mapping.h"

/**
 * @brief 游戏对象基类，封装名称、属性映射（CMapping）与基础 CRUD。
 */
class CMainObject {
public:
    const char* id(int flag = 0)
    {
        if (!this) return NULL;
        const char* mid = _T.querystr("apply/id");
        if (flag || !mid[0]) {
            return querystr("id");
        }

        return mid;
    }

    const char* name(int flag = 0)
    {
        if (!this) return NULL;
        const char* mname = _T.querystr("apply/name");
        if (flag || !mname[0]) {
            return querystr("name");
        }

        return mname;
    }

    void set_name(const char* name, const char* id = 0)
    {
        if (!this) return;
        if (name) {
            _M.set("name", name);
        }

        if (id) {
            // strncpy(m_szID, id, 40)[39] = 0;
            _M.set("id", id);
        }
        else {
            if (_M.undefinedp("id")) {
                _M.set("id", _M.querystr("base_name"));
            }
        }
    }

    void set(const char* key, int32_t value)
    {
        if (!this) return;
        _M.set(key, value);
    }

    void set(const char* key, const char* value)
    {
        if (!this) return;
        _M.set(key, value);
    }

    void set(const char* key, CMapping& value)
    {
        if (!this) return;
        _M.set(key, value);
    }

    void set(const char* key, CVector& value)
    {
        if (!this) return;
        _M.set(key, value);
    }

    int32_t query(const char* key)
    {
        if (!this) return 0;
        return _M.query(key);
    }

    const char* querystr(const char* key)
    {
        if (!this) return NULL;
        return _M.querystr(key);
    }

    CMapping* querymap(const char* key)
    {
        if (!this) return NULL;
        return _M.querymap(key);
    }

    CVector* queryvec(const char* key)
    {
        if (!this) return NULL;
        return _M.queryvec(key);
    }

    int32_t add(const char* key, int32_t value)
    {
        if (!this) return 0;
        return _M.add(key, value);
    }

    void del(const char* key)
    {
        if (!this) return;
        _M.del(key);
    }

    void clear()
    {
        if (!this) return;
        _M.clear();
    }

    void set_temp(const char* key, const char* value)
    {
        if (!this) return;
        _T.set(key, value);
    }

    void set_temp(const char* key, int32_t value)
    {
        if (!this) return;
        _T.set(key, value);
    }

    void set_temp(const char* key, CMapping& value)
    {
        if (!this) return;
        _T.set(key, value);
    }

    void set_temp(const char* key, CVector& value)
    {
        if (!this) return;
        _T.set(key, value);
    }

    const char* querystr_temp(const char* key)
    {
        if (!this) return NULL;
        return _T.querystr(key);
    }

    int32_t query_temp(const char* key)
    {
        if (!this) return 0;
        return _T.query(key);
    }

    CMapping* querymap_temp(const char* key)
    {
        if (!this) return NULL;
        return _T.querymap(key);
    }

    CVector* queryvec_temp(const char* key)
    {
        if (!this) return NULL;
        return _T.queryvec(key);
    }

    int32_t add_temp(const char* key, int32_t value)
    {
        if (!this) return 0;
        return _T.add(key, value);
    }

    void delete_temp(const char* key)
    {
        if (!this) return;
        _T.del(key);
    }

    virtual int32_t query_attr(int attr) { return 0; }

    CMainObject() {};

    virtual ~CMainObject() {};

    // Attribute
    enum {
        IS_SKILL,
        CAN_CAST_IN_COMBAT,
        CAN_CAST_IN_COMMON,

        // Item
        IS_ITEM,
        IS_FIGHTROOM,
        IS_CORPSE,
        CAN_USE,
        CAN_GET,
        CAN_COMBINE,
        CAN_EQUIPPED,
        CAN_SAVE,
        UPDATE,  // 版本更新标记

        // Equip
        IS_EQUIP,
        SELF_MAKE,

        // Combined
        IS_COMBINED,
        IS_MONEY,  // 是否钱

        // NPC
        IS_NPC,
        IS_VENDOR,

        // Container
        IS_CONTAINER,
        IS_INFINITY,  // 不受容量限制

        // Room
        IS_ROOM,
        IS_FBROOM,
        IS_FB,
        // User
        IS_USER,

        // Char
        IS_CHAR,
        IS_LIVING,
        IS_BUSY,
        IS_BANKER,
        IS_ZHANGMEN,

        // EXIT & DOOR
        IS_BOX,
        IS_RESIDENT,
        PERFORM,
        IS_MONSTER,

        IS_FOOD,

    };

    // GRANT
    enum {
        GRANT_NPC    = 10,
        GRANT_PLAYER = 30,
        GRANT_WIZARD = 50,
        GRANT_ADMIN  = 100
    };

protected:
    CMapping _M;
    CMapping _T;
};
