// SzCoreConfigManager.cpp -- native-backed SzConfigManager implementation.
#include "senzing/sdk/core/SzCoreConfigManager.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "native_ffi.hpp"
#include "senzing/sdk/core/SzCoreConfig.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"
#include "sz_helpers.hpp"

namespace senzing::sdk::core {

namespace {

// Default data sources excluded from auto-generated config comments (mirrors
// C# SzCoreUtilities.DefaultSources).
const std::set<std::string> kDefaultSources{"TEST", "SEARCH"};

// Advances past whitespace from `fromIndex` (mirrors SzCoreUtilities.EatWhiteSpace).
std::size_t EatWhiteSpace(std::string_view text, std::size_t fromIndex) {
    std::size_t index = fromIndex;
    while (index < text.size() &&
           std::isspace(static_cast<unsigned char>(text[index]))) {
        ++index;
    }
    return index;
}

// Generates a human-readable "Data Sources: ..." comment from a config
// definition by extracting the non-default DSRC_CODE values under CFG_DSRC.
// Faithful port of SzCoreUtilities.CreateConfigComment.
std::string CreateConfigComment(const std::string& configDefinition) {
    std::string_view text{configDefinition};

    std::size_t index = text.find("\"CFG_DSRC\"");
    if (index == std::string_view::npos) {
        return "";
    }
    index = EatWhiteSpace(text, index + std::string_view{"\"CFG_DSRC\""}.size());

    if (index >= text.size() || text[index++] != ':') {
        return "";
    }
    index = EatWhiteSpace(text, index);
    if (index >= text.size() || text[index++] != '[') {
        return "";
    }

    const std::size_t endIndex = text.find(']', index);
    if (endIndex == std::string_view::npos) {
        return "";
    }

    std::string result = "Data Sources: ";
    std::string prefix;
    int dataSourceCount = 0;
    std::set<std::string> defaultSources;

    while (index < endIndex) {
        const std::size_t found = text.find("\"DSRC_CODE\"", index);
        if (found == std::string_view::npos || found >= endIndex) {
            break;
        }
        index =
            EatWhiteSpace(text, found + std::string_view{"\"DSRC_CODE\""}.size());

        if (index >= endIndex || text[index++] != ':') {
            return "";
        }
        index = EatWhiteSpace(text, index);
        if (index >= endIndex || text[index++] != '"') {
            return "";
        }
        const std::size_t start = index;
        while (index < endIndex && text[index] != '"') {
            ++index;
        }
        if (index >= endIndex) {
            return "";
        }
        const std::string dataSource{text.substr(start, index - start)};
        if (kDefaultSources.count(dataSource) != 0) {
            defaultSources.insert(dataSource);
            continue;
        }
        result += prefix;
        result += dataSource;
        ++dataSourceCount;
        prefix = ", ";
    }

    if (dataSourceCount == 0 && defaultSources.empty()) {
        result += "[ NONE ]";
    } else if (dataSourceCount == 0 &&
               defaultSources.size() == kDefaultSources.size()) {
        result += "[ ONLY DEFAULT ]";
    } else if (dataSourceCount == 0) {
        result += "[ SOME DEFAULT (";
        prefix.clear();
        for (const std::string& source : defaultSources) {
            result += prefix;
            result += source;
            prefix = ", ";
        }
        result += ") ]";
    }

    return result;
}

}  // namespace

SzCoreConfigManager::SzCoreConfigManager(SzCoreEnvironment& env) : env_(env) {
    // Initialize the native SzConfig_* module (used by CreateConfig). The
    // SzConfigMgr_* module is initialized lazily on first use (mirrors C#).
    const int64_t returnCode =
        SzConfig_init(env_.GetInstanceName().c_str(),
                      env_.GetSettings().c_str(), env_.IsVerboseLogging() ? 1 : 0);
    CheckReturnCode(returnCode, SzModule::Config);
    configInitialized_ = true;
}

void SzCoreConfigManager::EnsureConfigMgrInitialized() {
    if (configMgrInitialized_) {
        return;
    }
    const int64_t returnCode = SzConfigMgr_init(
        env_.GetInstanceName().c_str(), env_.GetSettings().c_str(),
        env_.IsVerboseLogging() ? 1 : 0);
    CheckReturnCode(returnCode, SzModule::ConfigMgr);
    configMgrInitialized_ = true;
}

std::unique_ptr<SzConfig> SzCoreConfigManager::CreateConfig() {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    SzConfig_create_result createResult = SzConfig_create_helper();
    CheckReturnCode(createResult.returnCode, SzModule::Config);
    const uintptr_t handle = createResult.response;

    std::string configDef;
    {
        SzConfig_export_result exportResult = SzConfig_export_helper(handle);
        // Close the handle regardless of the export outcome, then propagate
        // any export error (mirrors the C# try/finally close).
        const int64_t closeCode = SzConfig_close_helper(handle);
        CheckReturnCode(exportResult.returnCode, SzModule::Config);
        configDef = TakeResponse(exportResult.response);
        CheckReturnCode(closeCode, SzModule::Config);
    }
    return std::make_unique<SzCoreConfig>(env_, std::move(configDef));
}

std::unique_ptr<SzConfig> SzCoreConfigManager::CreateConfig(
    const std::string& configDefinition) {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    SzConfig_load_result loadResult =
        SzConfig_load_helper(configDefinition.c_str());
    CheckReturnCode(loadResult.returnCode, SzModule::Config);
    const uintptr_t handle = reinterpret_cast<uintptr_t>(loadResult.response);

    std::string configDef;
    {
        SzConfig_export_result exportResult = SzConfig_export_helper(handle);
        const int64_t closeCode = SzConfig_close_helper(handle);
        CheckReturnCode(exportResult.returnCode, SzModule::Config);
        configDef = TakeResponse(exportResult.response);
        CheckReturnCode(closeCode, SzModule::Config);
    }
    return std::make_unique<SzCoreConfig>(env_, std::move(configDef));
}

std::unique_ptr<SzConfig> SzCoreConfigManager::CreateConfig(int64_t configID) {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureConfigMgrInitialized();
    SzConfigMgr_getConfig_result result = SzConfigMgr_getConfig_helper(configID);
    CheckReturnCode(result.returnCode, SzModule::ConfigMgr);
    std::string configDef = TakeResponse(result.response);
    return std::make_unique<SzCoreConfig>(env_, std::move(configDef));
}

int64_t SzCoreConfigManager::RegisterConfig(const std::string& configDefinition,
                                            const std::string& configComment) {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureConfigMgrInitialized();
    SzConfigMgr_registerConfig_result result = SzConfigMgr_registerConfig_helper(
        configDefinition.c_str(), configComment.c_str());
    CheckReturnCode(result.returnCode, SzModule::ConfigMgr);
    return result.configID;
}

int64_t SzCoreConfigManager::RegisterConfig(
    const std::string& configDefinition) {
    // Auto-generate the comment, then delegate to the two-arg overload.
    return RegisterConfig(configDefinition,
                          CreateConfigComment(configDefinition));
}

std::string SzCoreConfigManager::GetConfigRegistry() {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureConfigMgrInitialized();
    SzConfigMgr_getConfigRegistry_result result =
        SzConfigMgr_getConfigRegistry_helper();
    CheckReturnCode(result.returnCode, SzModule::ConfigMgr);
    return TakeResponse(result.response);
}

int64_t SzCoreConfigManager::GetDefaultConfigID() {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureConfigMgrInitialized();
    SzConfigMgr_getDefaultConfigID_result result =
        SzConfigMgr_getDefaultConfigID_helper();
    CheckReturnCode(result.returnCode, SzModule::ConfigMgr);
    return result.configID;
}

void SzCoreConfigManager::ReplaceDefaultConfigID(int64_t currentDefaultConfigID,
                                                 int64_t newDefaultConfigID) {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureConfigMgrInitialized();
    const int64_t returnCode = SzConfigMgr_replaceDefaultConfigID(
        currentDefaultConfigID, newDefaultConfigID);
    CheckReturnCode(returnCode, SzModule::ConfigMgr);
}

void SzCoreConfigManager::SetDefaultConfigID(int64_t configID) {
    env_.EnsureActive();
    std::lock_guard<std::mutex> guard(monitor_);
    EnsureConfigMgrInitialized();
    const int64_t returnCode = SzConfigMgr_setDefaultConfigID(configID);
    CheckReturnCode(returnCode, SzModule::ConfigMgr);
}

int64_t SzCoreConfigManager::SetDefaultConfig(
    const std::string& configDefinition, const std::string& configComment) {
    const int64_t configID = RegisterConfig(configDefinition, configComment);
    SetDefaultConfigID(configID);
    return configID;
}

int64_t SzCoreConfigManager::SetDefaultConfig(
    const std::string& configDefinition) {
    const int64_t configID = RegisterConfig(configDefinition);
    SetDefaultConfigID(configID);
    return configID;
}

void SzCoreConfigManager::Destroy() {
    std::lock_guard<std::mutex> guard(monitor_);
    if (destroyed_) {
        return;
    }
    if (configMgrInitialized_) {
        SzConfigMgr_destroy();
        configMgrInitialized_ = false;
    }
    if (configInitialized_) {
        SzConfig_destroy();
        configInitialized_ = false;
    }
    destroyed_ = true;
}

bool SzCoreConfigManager::IsDestroyed() {
    std::lock_guard<std::mutex> guard(monitor_);
    return destroyed_;
}

}  // namespace senzing::sdk::core
