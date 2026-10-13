#include <windows.h>

#include <stdio.h>

#include "Combatd.h"
#include "Channel.h"
#include "Global.h"
#include "User.h"

#include "IDatabase.h"
#include "Sql.h"
#include "IniParser.h"

// 脚本开机经 db_register_ddl 注册的建表语句（C++ 对其内容零知识）
std::vector<std::string> g_dataset_table_ddl;

bool prepare_tables(void)
{
    try {
        int result;

        const char* jianghudb
            = "CREATE TABLE IF NOT EXISTS `jianghudb` ("
              "`cc_id` int(32) NOT NULL AUTO_INCREMENT,"
              "`cc_name` varchar(40) NOT NULL DEFAULT '',"
              "`cc_passwd` varchar(22) NOT NULL DEFAULT '',"
              "`cc_email` varchar(64) NOT NULL DEFAULT '',"
              "`cc_str` int(16) NOT NULL DEFAULT '0',"
              "`cc_int` int(16) NOT NULL DEFAULT '0',"
              "`cc_con` int(16) NOT NULL DEFAULT '0',"
              "`cc_dex` int(16) NOT NULL DEFAULT '0',"
              "`cc_family` varchar(20) DEFAULT NULL,"
              "`cc_age` int(8) NOT NULL DEFAULT '0',"
              "`cc_gender` varchar(4) NOT NULL DEFAULT '',"
              "`cc_master` varchar(40) DEFAULT NULL,"
              "`cc_nickname` varchar(40) DEFAULT NULL,"
              "`cc_rank` varchar(40) DEFAULT NULL,"
              "`cc_class` varchar(16) DEFAULT NULL,"
              "`cc_level` tinyint(3) NOT NULL DEFAULT '0',"
              "`cc_experice` int(64) DEFAULT '0',"
              "`cc_savedata` blob,"
              "`cc_remark` int(16) NOT NULL DEFAULT '0',"
              "`cc_repute` int(16) NOT NULL DEFAULT '0',"
              "`cc_grant` tinyint(3) NOT NULL DEFAULT '30',"
              "`cc_score` int(16) NOT NULL DEFAULT '0',"
              "`cc_server` tinyint(3) NOT NULL DEFAULT '0',"
              "`cc_verifypass` varchar(20) NOT NULL DEFAULT '',"
              "`cc_ip` varchar(80) NOT NULL DEFAULT '',"
              "`cc_balance` int(16) NOT NULL DEFAULT '0',"
              "`cc_logintime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',"
              "`cc_savetime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',"
              "`cc_freeze` tinyint(1) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`cc_id`),"
              "KEY `cc_id` (`cc_id`),"
              "KEY `cc_grant` (`cc_grant`),"
              "KEY `cc_server` (`cc_server`),"
              "KEY `cc_experice` (`cc_experice`),"
              "KEY `ages` (`cc_age`),"
              "KEY `balances` (`cc_balance`),"
              "KEY `reputes` (`cc_repute`)"
              ")engine=InnoDB AUTO_INCREMENT=1000 default charset=utf8 "
              "AUTO_INCREMENT=1000 ;";

        result = g_db->execute(jianghudb);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: jianghudb fail!");
            return false;
        }

        const char* recruit_exp
            = "CREATE TABLE IF NOT EXISTS `recruit_exp` ("
              "`re_master` int(16) NOT NULL DEFAULT '0',"
              "`re_id` int(16) NOT NULL DEFAULT '0',"
              "`re_name` char(40) NOT NULL DEFAULT '',"
              "`re_exp` int(16) NOT NULL DEFAULT '0',"
              "`re_apprentice` tinyint(1) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`re_master`,`re_id`),"
              "KEY `re_master` (`re_master`,`re_id`)"
              ")engine=InnoDB default charset=utf8;";

        result = g_db->execute(recruit_exp);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: recruit_exp fail!");
            return false;
        }

        const char* t_item
            = "CREATE TABLE IF NOT EXISTS `t_item` ("
              "`obj_id` varchar(40) NOT NULL DEFAULT '',"
              "`obj_name` varchar(40) NOT NULL DEFAULT '',"
              "`obj_data` blob,"
              "`obj_lock` tinyint(1) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`obj_id`),"
              "KEY `obj_id` (`obj_id`)"
              ")engine=InnoDB default charset=utf8;";

        result = g_db->execute(t_item);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: t_item fail!");
            return false;
        }

        const char* t_npc
            = "CREATE TABLE IF NOT EXISTS `t_npc` ("
              "`obj_id` varchar(40) NOT NULL DEFAULT '',"
              "`obj_name` varchar(40) NOT NULL DEFAULT '',"
              "`obj_data` blob,"
              "`obj_lock` tinyint(1) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`obj_id`),"
              "KEY `obj_id` (`obj_id`)"
              ")engine=InnoDB default charset=utf8;";

        result = g_db->execute(t_npc);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: t_npc fail!");
            return false;
        }

        const char* t_room
            = "CREATE TABLE IF NOT EXISTS `t_room` ("
              "`obj_id` varchar(40) NOT NULL DEFAULT '',"
              "`obj_name` varchar(40) NOT NULL DEFAULT '',"
              "`obj_data` blob,"
              "`obj_lock` tinyint(1) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`obj_id`),"
              "KEY `obj_id` (`obj_id`)"
              ")engine=InnoDB default charset=utf8;";

        result = g_db->execute(t_room);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: t_room fail!");
            return false;
        }

        const char* t_saveobj
            = "CREATE TABLE IF NOT EXISTS `t_saveobj` ("
              "`owner` int(16) NOT NULL DEFAULT '0',"
              "`base_name` varchar(40) NOT NULL DEFAULT '',"
              "`savedata` blob NOT NULL,"
              "`grid` tinyint(2) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`grid`,`owner`),"
              "KEY `owner` (`owner`),"
              "KEY `base_name` (`base_name`)"
              ")engine=InnoDB default charset=utf8;";

        result = g_db->execute(t_saveobj);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: t_saveobj fail!");
            return false;
        }

        const char* t_box
            = "CREATE TABLE IF NOT EXISTS `t_box` ("
              "`obj_id` varchar(40) NOT NULL DEFAULT '',"
              "`obj_name` varchar(40) NOT NULL DEFAULT '',"
              "`obj_data` blob,"
              "`obj_lock` tinyint(1) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`obj_id`),"
              "KEY `obj_id` (`obj_id`)"
              ")engine=InnoDB default charset=utf8;";

        result = g_db->execute(t_box);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: t_box fail!");
            return false;
        }

        const char* user
            = "CREATE TABLE IF NOT EXISTS `user` ("
              "`id` int(16) NOT NULL AUTO_INCREMENT,"
              "`name` varchar(40) NOT NULL DEFAULT '',"
              "`passwd` varchar(10) NOT NULL DEFAULT '0',"
              "`email` varchar(64) NOT NULL DEFAULT '',"
              "`icqnumber` int(16) DEFAULT NULL,"
              "`phone` varchar(32) DEFAULT NULL,"
              "`handset` varchar(32) DEFAULT NULL,"
              "`bb` varchar(32) DEFAULT NULL,"
              "`homepage` varchar(128) DEFAULT NULL,"
              "`casehistory` varchar(255) DEFAULT NULL,"
              "PRIMARY KEY (`id`),"
              "KEY `id` (`id`)"
              ")engine=InnoDB default charset=utf8 AUTO_INCREMENT=1 ;";

        result = g_db->execute(user);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: user fail!");
            return false;
        }

        const char* mf
            = "CREATE TABLE IF NOT EXISTS `t_mf` ("
              "`id` int(16) NOT NULL AUTO_INCREMENT,"
              "`base_name` varchar(100) NOT NULL DEFAULT '',"
              "`count` int(64) NOT NULL DEFAULT '0',"
              "`energy` int(64) NOT NULL DEFAULT '0',"
              "PRIMARY KEY (`id`),"
              "KEY `id` (`id`)"
              ")engine=InnoDB default charset=utf8 AUTO_INCREMENT=1 ;";

        result = g_db->execute(mf);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: mf fail!");
            return false;
        }

        const char* t_record
            = "CREATE TABLE IF NOT EXISTS `t_record` ("
              "`id` int(16) NOT NULL AUTO_INCREMENT,"
              "`cc_time` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',"
              "`cc_id` int(16) NOT NULL DEFAULT '0',"
              "`cc_type` int(16) NOT NULL DEFAULT '0',"
              "`cc_level` int(16) NOT NULL DEFAULT '0',"
              "`cc_savedata` blob,"
              "PRIMARY KEY (`id`),"
              "KEY `id` (`id`)"
              ")engine=InnoDB default charset=utf8 AUTO_INCREMENT=1 ;";

        result = g_db->execute(t_record);
        if (result != 0) {
            LOG_ERROR("Prepare_tables: t_record fail!");
            return false;
        }

        // 数据集专用表：脚本注册的 DDL 逐条执行（表名/结构全归脚本）
        for (const std::string& ddl : g_dataset_table_ddl) {
            if (g_db->execute(ddl.c_str()) != 0) {
                LOG_ERROR("Prepare_tables: dataset table fail! %s",
                          ddl.c_str());
                return false;
            }
        }

        return true;
    } catch (...) {
        LOG_ERROR("Restore_Data:unknown!");
        return false;
    }
}
