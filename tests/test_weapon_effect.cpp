// test_weapon_effect.cpp - Unit tests for WeaponEffect.cpp
//
// Tests CWeaponEffect static methods with safe game state.
//
#include "pch.h"
#include "Char.h"
#include "Item.h"
#include "Room.h"
#include "WeaponEffect.h"

class WeaponEffectTest : public ::testing::Test {
protected:
    CChar* me;
    CChar* victim;
    CContainer* room;

    void SetUp() override
    {
        room = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "attacker");
        me->set("id", "attacker1");
        me->set("str", (long)20);
        me->move(room);

        victim = new CChar();
        victim->set("name", "defender");
        victim->set("id", "defender1");
        victim->set("str", (long)15);
        victim->move(room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        victim->move(nullptr);
        room->move(nullptr);
        delete me;
        delete victim;
        delete room;
    }
};

TEST_F(WeaponEffectTest, BashWeapon_NonParry_NoEffect)
{
    CWeaponEffect::bash_weapon(me, victim, nullptr, 10);
    // Non-parry damage → no weapon interaction
}

TEST_F(WeaponEffectTest, BashWeapon_NullWeapon)
{
    CWeaponEffect::bash_weapon(me, victim, nullptr, -2);
}

TEST_F(WeaponEffectTest, BashWeapon_NullVictimWeapon)
{
    CWeaponEffect::bash_weapon(me, victim, nullptr, -2);
}

TEST_F(WeaponEffectTest, ThrowWeapon_NullWeapon)
{
    CWeaponEffect::throw_weapon(me, victim, nullptr, 10);
}

TEST_F(WeaponEffectTest, ThrowWeapon_OwnedWeapon)
{
    CContainer* weapon = new CContainer();
    weapon->set("name", "长刀");
    weapon->set("id", "blade");
    weapon->set("owner", "attacker1");
    CWeaponEffect::throw_weapon(me, victim, weapon, 10);
    delete weapon;
}

TEST_F(WeaponEffectTest, ThrowWeapon_NoDuo)
{
    CContainer* weapon = new CContainer();
    weapon->set("name", "暗器");
    weapon->set("id", "dart");
    weapon->set("no_duo", (long)1);
    CWeaponEffect::throw_weapon(me, victim, weapon, 10);
    delete weapon;
}
