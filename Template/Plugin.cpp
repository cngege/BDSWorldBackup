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
#include <regex>
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
void GetBackupInfo_toBackUp();
void StartBackup(std::vector<std::string>);
void split(const std::string&, std::vector<std::string>&, const char);
void Trimmed_Regex(std::string&);
int createDirectory(std::string);


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
    
    TimerManager TM;
    Timer t(TM);
    //t.Start(Backup, config["TickTime"]);
    // TM.run();
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
        //return true;
        WorldBackupLogger.warn("开始执行加强版的命令");
        Level::runcmdEx("save hold");

        auto thread = std::thread([] {
            Sleep(5000);
            GetBackupInfo_toBackUp();
            //Bedrock level/db/001850.ldb:2134616, Bedrock level/db/001851.ldb:2125260, Bedrock level/db/001852.ldb:2116590, Bedrock level/db/001853.ldb:2116126, Bedrock level/db/001854.ldb:2121642, Bedrock level/db/001855.ldb:2135963, Bedrock level/db/001856.ldb:446661, Bedrock level/db/001909.log:0, Bedrock level/db/001910.ldb:202504, Bedrock level/db/CURRENT:16, Bedrock level/db/MANIFEST-001907:796, Bedrock level/level.dat:2508, Bedrock level/level.dat_old:2508, Bedrock level/levelname.txt:13
            });
        thread.detach();
        return true;
        });

    //auto info = "Bedrock level/db/001850.ldb:2134616, Bedrock level/db/001851.ldb:2125260, Bedrock level/db/001852.ldb:2116590, Bedrock level/db/001853.ldb:2116126, Bedrock level/db/001854.ldb:2121642, Bedrock level/db/001855.ldb:2135963, Bedrock level/db/001856.ldb:446661, Bedrock level/db/001909.log:0, Bedrock level/db/001910.ldb:202504, Bedrock level/db/CURRENT:16, Bedrock level/db/MANIFEST-001907:796, Bedrock level/level.dat:2508, Bedrock level/level.dat_old:2508, Bedrock level/levelname.txt:13";
    //std::vector<std::string> tokens;
    //split(info, tokens, ',');
    //for (auto &v : tokens) {
    //    Trimmed_Regex(v);
    //    WorldBackupLogger.info(v);
    //}
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
    //StartBackup();
}

/// <summary>
/// 获取备份列表后备份,否者不备份
/// </summary>
void GetBackupInfo_toBackUp()
{
    int i = 1;
    for (;;) {
        Sleep(5000);
        //n次查询均失败,则抛出错误
        if (i >= 6) {
            WorldBackupLogger.error("{}次查询 “save query 均执行失败,本次备份终止”", i);
            auto resume = Level::runcmdEx("save resume");
            if (!resume.first) {
                WorldBackupLogger.error("save resume命令执行失败,请手动运行命令以恢复修改");
            }
            return;
        }

        auto ret = Level::runcmdEx("save query");
        if (!ret.first){
            i++;
            continue;
        }
        if (i != 1) {
            WorldBackupLogger.info("第{}/6次查询成功,正在获取可备份文件列表信息,准备备份……");
        }
        else {
            WorldBackupLogger.info("查询成功,正在获取可备份文件列表信息,准备备份……");
        }

        // 将原始字符串分割出 可备份的文件部分+文件大小
        std::vector<std::string> t1;
        split(ret.second, t1, '\n');

        // 将可备份文件部分的每一条分割出来 file:size
        std::vector<std::string> t2;
        split(t1.at(1), t2, ',');

        auto thread = std::thread([t2] {
            StartBackup(t2);
            });
        thread.detach();
        break;
    }
    
    return;
}

/// <summary>
/// 正式开始备份存档
/// </summary>
/// <param name="info">可备份文件的信息</param>
void StartBackup(std::vector<std::string> info)
{
    //时间构成的文件夹名
    time_t now = time(0);
    tm* ltm = localtime(&now);
    std::string timeDir = std::to_string(ltm->tm_year+1900) + "_" + std::to_string(ltm->tm_mon+1) + "_" + std::to_string(ltm->tm_mday);
    timeDir += "__" + std::to_string(ltm->tm_hour) + "_" + std::to_string(ltm->tm_min) + "_" + std::to_string(ltm->tm_sec);
    
    //备份的目标文件夹
    std::string backup_to = (std::string)config["SavePath"] + timeDir + "/";
    if (config["Zip"]) {
        backup_to = (std::string)config["SavePath"] + "Tmp/";
    }

    //分割出文件部分和文件大小部分
    for (auto &f : info) {
        Trimmed_Regex(f);       // 去除头尾空格

        std::vector<std::string> t;
        split(f, t, ':');

        auto fp = t.at(0);
        auto size = std::stoi(t.at(1));

        //备份文件到目标地址 0x00000209a9b7e400 "Bedrock level/db/001850.ldb:2134616" "./plugins/BackUpMap/Tmp/"
        if (_access(std::string(backup_to + fp).c_str(), 0) == -1)	//表示配置文件所在的文件夹不存在
        {
            if (createDirectory(std::string(backup_to + fp).c_str()) == -1)
            {
                //文件夹创建失败
                WorldBackupLogger.error("备份文件时文件夹创建失败,请检查目标文件夹[{}]是否具有相关权限", backup_to + fp);
                WorldBackupLogger.error("本次备份失败,已终止");
                return;
            }
        }

        // 读取文件
        char* tempStr = new char[size];

        std::ifstream r(("./worlds/" + fp).c_str());
        r.read(tempStr, size);
        r.close();
        std::ofstream w((backup_to + fp).c_str());
        w.write(tempStr, size);
        w.close();
        delete[] tempStr;
    }
    
    //恢复修改
    auto resume = Level::runcmdEx("save resume");
    if (!resume.first) {
        WorldBackupLogger.error("备份完成，但save resume命令执行失败,请手动运行命令以恢复修改");
    }
    else {
        WorldBackupLogger.info("备份完成，已恢复修改");
    }

    //复制完成，判断是否需要压缩
    if (config["Zip"]) {
        WorldBackupLogger.info("准备创建压缩线程进行打包");
        WorldBackupLogger.info("压缩完成,本次备份结束");
    }
}

/// <summary>
/// 分割字符串
/// </summary>
/// <param name="str">要分割的字符串</param>
/// <param name="tokens">分割后的字符串数组,输出</param>
/// <param name="delim">分割字符</param>
void split(const std::string& str,
    std::vector<std::string>& tokens,
    const char delim = ' ') {
    tokens.clear();

    std::istringstream iss(str);
    std::string tmp;
    while (std::getline(iss, tmp, delim)) {
        if (tmp != "") {
            // 如果两个分隔符相邻，则 tmp == ""，忽略。
            tokens.emplace_back(std::move(tmp));
        }
    }
}

/// <summary>
/// 正则去除字符串头尾空格
/// </summary>
/// <param name="str"></param>
void Trimmed_Regex(std::string& str) {
    std::regex e("([\\S].*[\\S])");
    str = std::regex_replace(str, e, "$1", std::regex_constants::format_no_copy);
}


/// <summary>
/// 创建多级目录 原来的_mkdir只能一层一层的创建
/// </summary>
/// <param name="path"></param>
/// <returns></returns>
int createDirectory(std::string path)
{
    int len = path.length();
    char tmpDirPath[256] = { 0 };
    for (int i = 0; i < len; i++)
    {
        tmpDirPath[i] = path[i];
        if (tmpDirPath[i] == '\\' || tmpDirPath[i] == '/')
        {
            if (_access(tmpDirPath, 0) == -1)
            {
                int ret = _mkdir(tmpDirPath);
                if (ret == -1) return ret;
            }
        }
    }
    return 0;
}