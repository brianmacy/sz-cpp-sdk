// SzEngine.hpp -- abstract interface mirroring C# Senzing.Sdk.SzEngine.
//
// Full public surface. Flag parameters default to the per-method default
// constant from SzFlags.hpp (generated from szflags.json), exactly as the C#
// SDK does. The export handle mirrors the C# `IntPtr` as an opaque raw handle.
//
// Methods marked `[SzConfigRetryable]` may throw SzConfigurationException when
// the active config is stale; the call is safe to retry after reinitialization.
// (In C# this is an attribute; here it is a documented convention.)
#ifndef SENZING_SDK_SZENGINE_HPP
#define SENZING_SDK_SZENGINE_HPP

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "senzing/sdk/SzFlags.hpp"

namespace senzing::sdk {

/// Opaque export-report handle, mirroring the C# `IntPtr`. The raw native
/// handle is exposed deliberately (no RAII wrapper) so the surface matches C#;
/// callers must pass it to CloseExportReport.
using SzExportHandle = std::uintptr_t;

/// The core Senzing entity-resolution engine.
class SzEngine {
public:
    virtual ~SzEngine() = default;

    /// Pre-loads engine resources to reduce first-call latency.
    virtual void PrimeEngine() = 0;

    /// Returns JSON workload statistics for this engine instance.
    virtual std::string GetStats() = 0;

    // ---- Data manipulation ----
    // [SzConfigRetryable]
    virtual std::string AddRecord(const std::string& dataSourceCode,
                                  const std::string& recordID,
                                  const std::string& recordDefinition,
                                  SzFlags flags = SzAddRecordDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string GetRecordPreview(
        const std::string& recordDefinition,
        SzFlags flags = SzRecordPreviewDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string DeleteRecord(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzDeleteRecordDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string ReevaluateRecord(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzReevaluateRecordDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string ReevaluateEntity(
        int64_t entityID, SzFlags flags = SzReevaluateEntityDefaultFlags) = 0;

    // ---- Search ----
    // [SzConfigRetryable]
    virtual std::string SearchByAttributes(
        const std::string& attributes, const std::string& searchProfile,
        SzFlags flags = SzSearchByAttributesDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string SearchByAttributes(
        const std::string& attributes,
        SzFlags flags = SzSearchByAttributesDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string WhySearch(const std::string& attributes,
                                  int64_t entityID,
                                  const std::string& searchProfile = "",
                                  SzFlags flags = SzWhySearchDefaultFlags) = 0;

    // ---- Retrieval ----
    // [SzConfigRetryable]
    virtual std::string GetEntity(int64_t entityID,
                                  SzFlags flags = SzEntityDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string GetEntity(const std::string& dataSourceCode,
                                  const std::string& recordID,
                                  SzFlags flags = SzEntityDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string GetRecord(const std::string& dataSourceCode,
                                  const std::string& recordID,
                                  SzFlags flags = SzRecordDefaultFlags) = 0;

    // ---- Interesting entities ----
    // [SzConfigRetryable]
    virtual std::string FindInterestingEntities(
        int64_t entityID,
        SzFlags flags = SzFindInterestingEntitiesDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string FindInterestingEntities(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzFindInterestingEntitiesDefaultFlags) = 0;

    // ---- Path ----
    // [SzConfigRetryable]
    virtual std::string FindPath(
        int64_t startEntityID, int64_t endEntityID, int32_t maxDegrees,
        const std::set<int64_t>& avoidEntityIDs = {},
        const std::set<std::string>& requiredDataSources = {},
        SzFlags flags = SzFindPathDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string FindPath(
        const std::string& startDataSourceCode, const std::string& startRecordID,
        const std::string& endDataSourceCode, const std::string& endRecordID,
        int32_t maxDegrees,
        const std::set<std::pair<std::string, std::string>>& avoidRecordKeys = {},
        const std::set<std::string>& requiredDataSources = {},
        SzFlags flags = SzFindPathDefaultFlags) = 0;

    // ---- Network ----
    // [SzConfigRetryable]
    virtual std::string FindNetwork(
        const std::set<int64_t>& entityIDs, int32_t maxDegrees,
        int32_t buildOutDegrees, int32_t buildOutMaxEntities,
        SzFlags flags = SzFindNetworkDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string FindNetwork(
        const std::set<std::pair<std::string, std::string>>& recordKeys,
        int32_t maxDegrees, int32_t buildOutDegrees, int32_t buildOutMaxEntities,
        SzFlags flags = SzFindNetworkDefaultFlags) = 0;

    // ---- Why / How / Virtual ----
    // [SzConfigRetryable]
    virtual std::string WhyRecordInEntity(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzWhyRecordInEntityDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string WhyRecords(
        const std::string& dataSourceCode1, const std::string& recordID1,
        const std::string& dataSourceCode2, const std::string& recordID2,
        SzFlags flags = SzWhyRecordsDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string WhyEntities(
        int64_t entityID1, int64_t entityID2,
        SzFlags flags = SzWhyEntitiesDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string HowEntity(int64_t entityID,
                                  SzFlags flags = SzHowEntityDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string GetVirtualEntity(
        const std::set<std::pair<std::string, std::string>>& recordKeys,
        SzFlags flags = SzVirtualEntityDefaultFlags) = 0;

    // ---- Export ----
    // [SzConfigRetryable]
    virtual SzExportHandle ExportJsonEntityReport(
        SzFlags flags = SzExportDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual SzExportHandle ExportCsvEntityReport(
        const std::string& csvColumnList, SzFlags flags = SzExportDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string FetchNext(SzExportHandle exportHandle) = 0;

    virtual void CloseExportReport(SzExportHandle exportHandle) = 0;

    // ---- Redo ----
    // [SzConfigRetryable]
    virtual std::string ProcessRedoRecord(const std::string& redoRecord,
                                          SzFlags flags = SzRedoDefaultFlags) = 0;

    // [SzConfigRetryable]
    virtual std::string GetRedoRecord() = 0;

    virtual int64_t CountRedoRecords() = 0;
};

}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZENGINE_HPP
