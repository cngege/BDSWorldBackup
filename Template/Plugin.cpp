#include "pch.h"
#include <EventAPI.h>
#include <LoggerAPI.h>
#include <MC/Level.hpp>
#include <MC/BlockInstance.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <MC/Actor.hpp>
#include <MC/Player.hpp>
#include <MC/ItemStack.hpp>
#include "Version.h"
#include <LLAPI.h>
#include <ServerAPI.h>
#include <MC/Json.hpp>
#include <io.h>
#include <direct.h>
#include <filesystem>
//定时器组件
#include "CTimer.cpp"
Logger WorldBackupLogger(PLUGIN_NAME);


string configpath = "./plugins/BackUpMap/";
string bakcupPath = "";                                //存档默认备份路径
string temp       = "";                                //如果要压缩备份、存档的临时存储目录

bool haveHadPlayer = false;

json config = R"(
{
    "TickTime"   : 7200000,
    "SavePath"   : "./plugins/BackUpMap/",
    "NeedPlayer" : true,
    "Zip"        : true,
    "CheckingDiskSpace"  : true,
    "FreeSpace"  : 1073741824,
    "Run"        : ""
}
)"_json;


void Backup();
void StartBackup();

inline void CheckProtocolVersion() {
#ifdef TARGET_BDS_PROTOCOL_VERSION
    auto currentProtocol = LL::getServerProtocolVersion();
    if (TARGET_BDS_PROTOCOL_VERSION != currentProtocol)
    {
        logger.warn("Protocol version not match, target version: {}, current version: {}.",
            TARGET_BDS_PROTOCOL_VERSION, currentProtocol);
        logger.warn("This will most likely crash the server, please use the Plugin that matches the BDS version!");
    }
#endif // TARGET_BDS_PROTOCOL_VERSION
}



void PluginInit()
{
    CheckProtocolVersion();
    //判断是否存在配置文件 没有则创建，有则读取并保存到config
    //检查配置文件夹是否存在
    if (_access(configpath.c_str(), 0) == -1)	//表示配置文件所在的文件夹不存在
    {
        if (_mkdir(configpath.c_str()) == -1)
        {
            //文件夹创建失败
            WorldBackupLogger.error("文件夹创建失败,请手动创建文件夹后重试 {}", configpath);
            return;
        }
    }
    std::ifstream f((configpath + "BackupMap.json").c_str());
    if (f.good())								//表示配置文件存在
    {
        f >> config;
        f.close();
    }
    else {
        //配置文件不存在
        WorldBackupLogger.warn("配置文件[{}]不存在,将自动生成并写入默认配置", "BackupMap.json");
        std::ofstream c((configpath + "BackupMap.json").c_str());
        c << config.dump(2);
        c.close();
    }

    //启动一个定时器,定时备份存档
    
    TimerManager pTimer;
    Timer t(pTimer);
    //t.Start(Backup, config["TickTime"]);
    //t.Stop();

    //玩家加入服务器事件
    if (config["NeedPlayer"])
    {
        Event::PlayerJoinEvent::subscribe([](const Event::PlayerJoinEvent& e) {
            haveHadPlayer = true;
            return true;
        });
    }

    Event::ServerStartedEvent::subscribe([](const Event::ServerStartedEvent& e) {
        WorldBackupLogger.warn("开始执行加强版的命令");
        std::ofstream cm((configpath + "BackupMap.txt").c_str());
        auto ret = Level::runcmdEx("save hold");
        cm << "save hold:" << ret.first ? "is true" : "is false";
        cm << "save hold:" << ret.second;
        cm << "=======";
        Sleep(5000);
        auto ret2 = Level::runcmdEx("save query");
        cm << "save query:" << ret2.first ? "is true" : "is false";
        cm << "save query:" << ret2.second;
        cm << "=======";
        auto ret3 = Level::runcmdEx("save resume");
        cm << "save resume:" << ret3.first ? "is true" : "is false";
        cm << "save resume:" << ret3.second;
        cm << "=======";
        cm.close();
        return true;
        });
}


/// <summary>
/// 检查并备份存档
/// </summary>
void Backup()
{
    //检查 服务器剩余空间
    if (config["CheckingDiskSpace"])
    {
        std::string _savePath = config["SavePath"];
        std::filesystem::path p{ _savePath };
        uintmax_t free = std::filesystem::space(p).free;
        //如果剩余空间 小于设定值,输出警告并结束备份
        if (free < config["FreeSpace"])
        {
            WorldBackupLogger.warn("剩余空间[{}MB]不足,本次备份中断", free / 1024 / 1024);
            return;
        }
    }

    //检查NeedPlayer开关,并检查是否符合条件
    if (config["NeedPlayer"])
    {
        //检查 从上次备份到现在是否有玩家进入过服务器，或此段时间服务器是否有过玩家
        if (!haveHadPlayer)
        {
            return;
        }
        //发起备份的时候 服务器没有玩家 则将 haveHadPlayer 置为false
        if (Level::getAllPlayers().size() == 0)
        {
            haveHadPlayer = false;
        }
    }
    StartBackup();
}

/// <summary>
/// 正式开始备份存档
/// </summary>
void StartBackup()
{
    auto ret = Level::runcmdEx("");
    //ret.first 命令是否成功达到预期效果
    //ret.second 命令输出
}