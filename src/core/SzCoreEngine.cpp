// SzCoreEngine.cpp -- native-backed SzEngine implementation.
//
// Mirrors core/SzCoreEngine.cs. The C# SDK masks out the SzWithInfo bit before
// passing flags downstream (SdkFlagMask = ~SzWithInfo); we do the same. Because
// every SDK method always passes flags and the data-mutation methods return the
// WithInfo JSON, the called native variant is always the highest one carrying
// all args (`_V2`/`_V3`/`*WithInfo_helper`).
#include "senzing/sdk/core/SzCoreEngine.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include "native_ffi.hpp"
#include "senzing/sdk/core/SzCoreEnvironment.hpp"
#include "sz_helpers.hpp"

namespace senzing::sdk::core {

namespace {

// SDK-only flag bit (mirrors C# SzFlag.SzWithInfo == 1L << 62). The native
// helpers do not understand it, so it is masked out of downstream flags.
constexpr int64_t kSzWithInfoBit = 1LL << 62;

// Strips SDK-specific bits, leaving only flags the native layer understands.
// Mirrors C# `FlagsToLong(flags) & SdkFlagMask`.
constexpr int64_t DownstreamFlags(SzFlags flags) noexcept {
    return flags.Value() & ~kSzWithInfoBit;
}

// Escapes a string as a JSON string literal (including surrounding quotes).
// Mirrors C# Utilities.JsonEscape (also used in SzCoreConfig.cpp).
std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char ch : value) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    static const char* kHex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(kHex[(ch >> 4) & 0xF]);
                    out.push_back(kHex[ch & 0xF]);
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
    return out;
}

}  // namespace

// ---- Encode helpers (mirror C# EncodeEntityIDs/EncodeRecordKeys/EncodeDataSources) ----

std::string SzCoreEngine::EncodeEntityIDs(const std::set<int64_t>& entityIDs) {
    std::string out = "{\"ENTITIES\":[";
    const char* prefix = "";
    for (const int64_t entityID : entityIDs) {
        out += prefix;
        out += "{\"ENTITY_ID\":";
        out += std::to_string(entityID);
        out += "}";
        prefix = ",";
    }
    out += "]}";
    return out;
}

std::string SzCoreEngine::EncodeRecordKeys(
    const std::set<std::pair<std::string, std::string>>& recordKeys) {
    std::string out = "{\"RECORDS\":[";
    const char* prefix = "";
    for (const auto& [dataSourceCode, recordID] : recordKeys) {
        out += prefix;
        out += "{\"DATA_SOURCE\":";
        out += JsonEscape(dataSourceCode);
        out += ",\"RECORD_ID\":";
        out += JsonEscape(recordID);
        out += "}";
        prefix = ",";
    }
    out += "]}";
    return out;
}

std::string SzCoreEngine::EncodeDataSources(
    const std::set<std::string>& dataSources) {
    std::string out = "{\"DATA_SOURCES\":[";
    const char* prefix = "";
    for (const auto& dataSourceCode : dataSources) {
        out += prefix;
        out += JsonEscape(dataSourceCode);
        prefix = ",";
    }
    out += "]}";
    return out;
}

// ---- Construction / lifecycle ----

SzCoreEngine::SzCoreEngine(SzCoreEnvironment& env) : env_(env) {
    // Choose the native init path based on whether the environment pins an
    // explicit config ID (mirrors C# SzCoreEngine ctor: configID == null uses
    // Init, otherwise InitWithConfigID).
    const std::optional<int64_t> configID = env_.GetConfigID();
    if (configID.has_value()) {
        const int64_t returnCode = Sz_initWithConfigID(
            env_.GetInstanceName().c_str(), env_.GetSettings().c_str(),
            *configID, env_.IsVerboseLogging() ? 1 : 0);
        CheckReturnCode(returnCode, SzModule::Engine);
    } else {
        const int64_t returnCode =
            Sz_init(env_.GetInstanceName().c_str(), env_.GetSettings().c_str(),
                    env_.IsVerboseLogging() ? 1 : 0);
        CheckReturnCode(returnCode, SzModule::Engine);
    }
}

void SzCoreEngine::Destroy() {
    std::lock_guard<std::mutex> guard(monitor_);
    if (destroyed_) {
        return;
    }
    Sz_destroy();
    destroyed_ = true;
}

bool SzCoreEngine::IsDestroyed() {
    std::lock_guard<std::mutex> guard(monitor_);
    return destroyed_;
}

// ---- Lifecycle / stats ----

void SzCoreEngine::PrimeEngine() {
    env_.EnsureActive();
    const int64_t returnCode = Sz_primeEngine();
    CheckReturnCode(returnCode, SzModule::Engine);
}

std::string SzCoreEngine::GetStats() {
    env_.EnsureActive();
    Sz_stats_result result = Sz_stats_helper();
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Data manipulation (always WithInfo: SDK methods return the info JSON) ----

std::string SzCoreEngine::AddRecord(const std::string& dataSourceCode,
                                    const std::string& recordID,
                                    const std::string& recordDefinition,
                                    SzFlags flags) {
    env_.EnsureActive();
    Sz_addRecordWithInfo_result result = Sz_addRecordWithInfo_helper(
        dataSourceCode.c_str(), recordID.c_str(), recordDefinition.c_str(),
        DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::GetRecordPreview(const std::string& recordDefinition,
                                           SzFlags flags) {
    env_.EnsureActive();
    Sz_getRecordPreview_result result = Sz_getRecordPreview_helper(
        recordDefinition.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::DeleteRecord(const std::string& dataSourceCode,
                                       const std::string& recordID,
                                       SzFlags flags) {
    env_.EnsureActive();
    Sz_deleteRecordWithInfo_result result = Sz_deleteRecordWithInfo_helper(
        dataSourceCode.c_str(), recordID.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::ReevaluateRecord(const std::string& dataSourceCode,
                                           const std::string& recordID,
                                           SzFlags flags) {
    env_.EnsureActive();
    Sz_reevaluateRecordWithInfo_result result =
        Sz_reevaluateRecordWithInfo_helper(dataSourceCode.c_str(),
                                           recordID.c_str(),
                                           DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::ReevaluateEntity(int64_t entityID, SzFlags flags) {
    env_.EnsureActive();
    Sz_reevaluateEntityWithInfo_result result =
        Sz_reevaluateEntityWithInfo_helper(entityID, DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Search ----

std::string SzCoreEngine::SearchByAttributes(const std::string& attributes,
                                             const std::string& searchProfile,
                                             SzFlags flags) {
    // An empty searchProfile means "use the default profile". C# treats a null
    // profile as the V2 default-profile path (SearchByAttributes(attrs, flags)),
    // so an empty C++ profile must delegate there rather than passing "" to the
    // V3 helper (which would request a profile literally named ""). A non-empty
    // profile uses the V3 helper carrying the explicit profile.
    if (searchProfile.empty()) {
        return SearchByAttributes(attributes, flags);
    }
    env_.EnsureActive();
    Sz_searchByAttributes_V3_result result = Sz_searchByAttributes_V3_helper(
        attributes.c_str(), searchProfile.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::SearchByAttributes(const std::string& attributes,
                                             SzFlags flags) {
    env_.EnsureActive();
    // No profile -> V2 helper (the C# `searchProfile == null` branch).
    Sz_searchByAttributes_V2_result result =
        Sz_searchByAttributes_V2_helper(attributes.c_str(),
                                        DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::WhySearch(const std::string& attributes,
                                    int64_t entityID,
                                    const std::string& searchProfile,
                                    SzFlags flags) {
    env_.EnsureActive();
    // C# passes searchProfile (possibly null) to the V2 helper. An empty profile
    // means "default profile" -> pass nullptr to match the C# null semantics.
    const char* profile = searchProfile.empty() ? nullptr : searchProfile.c_str();
    Sz_whySearch_V2_result result = Sz_whySearch_V2_helper(
        attributes.c_str(), entityID, profile, DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Retrieval ----

std::string SzCoreEngine::GetEntity(int64_t entityID, SzFlags flags) {
    env_.EnsureActive();
    Sz_getEntityByEntityID_V2_result result =
        Sz_getEntityByEntityID_V2_helper(entityID, DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::GetEntity(const std::string& dataSourceCode,
                                    const std::string& recordID,
                                    SzFlags flags) {
    env_.EnsureActive();
    Sz_getEntityByRecordID_V2_result result = Sz_getEntityByRecordID_V2_helper(
        dataSourceCode.c_str(), recordID.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::GetRecord(const std::string& dataSourceCode,
                                    const std::string& recordID,
                                    SzFlags flags) {
    env_.EnsureActive();
    Sz_getRecord_V2_result result = Sz_getRecord_V2_helper(
        dataSourceCode.c_str(), recordID.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Interesting entities ----

std::string SzCoreEngine::FindInterestingEntities(int64_t entityID,
                                                  SzFlags flags) {
    env_.EnsureActive();
    Sz_findInterestingEntitiesByEntityID_result result =
        Sz_findInterestingEntitiesByEntityID_helper(entityID,
                                                    DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::FindInterestingEntities(
    const std::string& dataSourceCode, const std::string& recordID,
    SzFlags flags) {
    env_.EnsureActive();
    Sz_findInterestingEntitiesByRecordID_result result =
        Sz_findInterestingEntitiesByRecordID_helper(
            dataSourceCode.c_str(), recordID.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Path (select the native variant by which optional args are present) ----

std::string SzCoreEngine::FindPath(
    int64_t startEntityID, int64_t endEntityID, int32_t maxDegrees,
    const std::set<int64_t>& avoidEntityIDs,
    const std::set<std::string>& requiredDataSources, SzFlags flags) {
    env_.EnsureActive();
    const int64_t downstreamFlags = DownstreamFlags(flags);
    if (avoidEntityIDs.empty() && requiredDataSources.empty()) {
        Sz_findPathByEntityID_V2_result result = Sz_findPathByEntityID_V2_helper(
            startEntityID, endEntityID, maxDegrees, downstreamFlags);
        CheckReturnCode(result.returnCode, SzModule::Engine);
        return TakeResponse(result.response);
    }
    if (requiredDataSources.empty()) {
        const std::string avoidanceJson = EncodeEntityIDs(avoidEntityIDs);
        Sz_findPathByEntityIDWithAvoids_V2_result result =
            Sz_findPathByEntityIDWithAvoids_V2_helper(
                startEntityID, endEntityID, maxDegrees, avoidanceJson.c_str(),
                downstreamFlags);
        CheckReturnCode(result.returnCode, SzModule::Engine);
        return TakeResponse(result.response);
    }
    const std::string avoidanceJson = EncodeEntityIDs(avoidEntityIDs);
    const std::string dataSourceJson = EncodeDataSources(requiredDataSources);
    Sz_findPathByEntityIDIncludingSource_V2_result result =
        Sz_findPathByEntityIDIncludingSource_V2_helper(
            startEntityID, endEntityID, maxDegrees, avoidanceJson.c_str(),
            dataSourceJson.c_str(), downstreamFlags);
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::FindPath(
    const std::string& startDataSourceCode, const std::string& startRecordID,
    const std::string& endDataSourceCode, const std::string& endRecordID,
    int32_t maxDegrees,
    const std::set<std::pair<std::string, std::string>>& avoidRecordKeys,
    const std::set<std::string>& requiredDataSources, SzFlags flags) {
    env_.EnsureActive();
    const int64_t downstreamFlags = DownstreamFlags(flags);
    if (avoidRecordKeys.empty() && requiredDataSources.empty()) {
        Sz_findPathByRecordID_V2_result result = Sz_findPathByRecordID_V2_helper(
            startDataSourceCode.c_str(), startRecordID.c_str(),
            endDataSourceCode.c_str(), endRecordID.c_str(), maxDegrees,
            downstreamFlags);
        CheckReturnCode(result.returnCode, SzModule::Engine);
        return TakeResponse(result.response);
    }
    if (requiredDataSources.empty()) {
        const std::string avoidanceJson = EncodeRecordKeys(avoidRecordKeys);
        Sz_findPathByRecordIDWithAvoids_V2_result result =
            Sz_findPathByRecordIDWithAvoids_V2_helper(
                startDataSourceCode.c_str(), startRecordID.c_str(),
                endDataSourceCode.c_str(), endRecordID.c_str(), maxDegrees,
                avoidanceJson.c_str(), downstreamFlags);
        CheckReturnCode(result.returnCode, SzModule::Engine);
        return TakeResponse(result.response);
    }
    const std::string avoidanceJson = EncodeRecordKeys(avoidRecordKeys);
    const std::string dataSourceJson = EncodeDataSources(requiredDataSources);
    Sz_findPathByRecordIDIncludingSource_V2_result result =
        Sz_findPathByRecordIDIncludingSource_V2_helper(
            startDataSourceCode.c_str(), startRecordID.c_str(),
            endDataSourceCode.c_str(), endRecordID.c_str(), maxDegrees,
            avoidanceJson.c_str(), dataSourceJson.c_str(), downstreamFlags);
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Network ----

std::string SzCoreEngine::FindNetwork(const std::set<int64_t>& entityIDs,
                                      int32_t maxDegrees,
                                      int32_t buildOutDegrees,
                                      int32_t buildOutMaxEntities,
                                      SzFlags flags) {
    env_.EnsureActive();
    const std::string jsonEntityIDs = EncodeEntityIDs(entityIDs);
    Sz_findNetworkByEntityID_V2_result result =
        Sz_findNetworkByEntityID_V2_helper(jsonEntityIDs.c_str(), maxDegrees,
                                           buildOutDegrees, buildOutMaxEntities,
                                           DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::FindNetwork(
    const std::set<std::pair<std::string, std::string>>& recordKeys,
    int32_t maxDegrees, int32_t buildOutDegrees, int32_t buildOutMaxEntities,
    SzFlags flags) {
    env_.EnsureActive();
    const std::string jsonRecordKeys = EncodeRecordKeys(recordKeys);
    // NOTE: call the *ByRecordID* helper. The C# FindNetworkByRecordID wrapper
    // has a copy/paste bug that calls the ByEntityID helper; we do not replicate
    // that bug (see docs/native-ffi-contract.md).
    Sz_findNetworkByRecordID_V2_result result =
        Sz_findNetworkByRecordID_V2_helper(jsonRecordKeys.c_str(), maxDegrees,
                                           buildOutDegrees, buildOutMaxEntities,
                                           DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Why / How / Virtual ----

std::string SzCoreEngine::WhyRecordInEntity(const std::string& dataSourceCode,
                                            const std::string& recordID,
                                            SzFlags flags) {
    env_.EnsureActive();
    Sz_whyRecordInEntity_V2_result result = Sz_whyRecordInEntity_V2_helper(
        dataSourceCode.c_str(), recordID.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::WhyRecords(const std::string& dataSourceCode1,
                                     const std::string& recordID1,
                                     const std::string& dataSourceCode2,
                                     const std::string& recordID2,
                                     SzFlags flags) {
    env_.EnsureActive();
    Sz_whyRecords_V2_result result = Sz_whyRecords_V2_helper(
        dataSourceCode1.c_str(), recordID1.c_str(), dataSourceCode2.c_str(),
        recordID2.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::WhyEntities(int64_t entityID1, int64_t entityID2,
                                      SzFlags flags) {
    env_.EnsureActive();
    Sz_whyEntities_V2_result result =
        Sz_whyEntities_V2_helper(entityID1, entityID2, DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::HowEntity(int64_t entityID, SzFlags flags) {
    env_.EnsureActive();
    Sz_howEntityByEntityID_V2_result result =
        Sz_howEntityByEntityID_V2_helper(entityID, DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::GetVirtualEntity(
    const std::set<std::pair<std::string, std::string>>& recordKeys,
    SzFlags flags) {
    env_.EnsureActive();
    const std::string jsonRecordKeys = EncodeRecordKeys(recordKeys);
    Sz_getVirtualEntityByRecordID_V2_result result =
        Sz_getVirtualEntityByRecordID_V2_helper(jsonRecordKeys.c_str(),
                                                DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

// ---- Export ----

SzExportHandle SzCoreEngine::ExportJsonEntityReport(SzFlags flags) {
    env_.EnsureActive();
    Sz_exportJSONEntityReport_result result =
        Sz_exportJSONEntityReport_helper(DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return result.exportHandle;
}

SzExportHandle SzCoreEngine::ExportCsvEntityReport(
    const std::string& csvColumnList, SzFlags flags) {
    env_.EnsureActive();
    Sz_exportCSVEntityReport_result result = Sz_exportCSVEntityReport_helper(
        csvColumnList.c_str(), DownstreamFlags(flags));
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return result.exportHandle;
}

std::string SzCoreEngine::FetchNext(SzExportHandle exportHandle) {
    env_.EnsureActive();
    // A null response signals end-of-stream; TakeResponse maps that to an empty
    // string, matching the C# behavior of returning null/empty at EOF.
    Sz_fetchNext_result result = Sz_fetchNext_helper(exportHandle);
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

void SzCoreEngine::CloseExportReport(SzExportHandle exportHandle) {
    env_.EnsureActive();
    // Export handles are released by their dedicated close fn, NOT SzHelper_free.
    const int64_t returnCode = Sz_closeExportReport_helper(exportHandle);
    CheckReturnCode(returnCode, SzModule::Engine);
}

// ---- Redo ----

std::string SzCoreEngine::ProcessRedoRecord(const std::string& redoRecord,
                                            SzFlags /*flags*/) {
    env_.EnsureActive();
    // The native processRedoRecordWithInfo helper takes no flags argument (the
    // info is implied); the SDK flags param is accepted for API symmetry only.
    Sz_processRedoRecordWithInfo_result result =
        Sz_processRedoRecordWithInfo_helper(redoRecord.c_str());
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

std::string SzCoreEngine::GetRedoRecord() {
    env_.EnsureActive();
    Sz_getRedoRecord_result result = Sz_getRedoRecord_helper();
    CheckReturnCode(result.returnCode, SzModule::Engine);
    return TakeResponse(result.response);
}

int64_t SzCoreEngine::CountRedoRecords() {
    env_.EnsureActive();
    const int64_t count = Sz_countRedoRecords();
    // Per the native ABI a negative value signals failure; non-negative is the
    // count. Mirror the C# pattern of treating a negative result as an error.
    if (count < 0) {
        CheckReturnCode(count, SzModule::Engine);
    }
    return count;
}

}  // namespace senzing::sdk::core
