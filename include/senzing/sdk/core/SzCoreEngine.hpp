// SzCoreEngine.hpp -- native-backed SzEngine implementation.
//
// Ports core/SzCoreEngine.cs. Owns the native engine (`Sz_*`) module,
// initialized with the owning environment's settings. Constructed and
// lifecycle-managed by SzCoreEnvironment (obtain via GetEngine).
//
// Flag handling mirrors the C# SDK: the SzWithInfo bit is an SDK-level flag and
// is masked out of the value passed to the native helpers (SdkFlagMask). Every
// SDK method always passes flags, so the highest native variant carrying all
// args (the `_V2`/`_V3`/`*WithInfo_helper` forms) is the one called.
#ifndef SENZING_SDK_CORE_SZCOREENGINE_HPP
#define SENZING_SDK_CORE_SZCOREENGINE_HPP

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#include "senzing/sdk/SzEngine.hpp"

namespace senzing::sdk::core {

class SzCoreEnvironment;

/// Core implementation of SzEngine backed by the native `Sz_*` module.
class SzCoreEngine final : public SzEngine {
public:
    /// Constructs and initializes the native engine module using the owning
    /// environment's settings. Throws the mapped SzException on init failure.
    explicit SzCoreEngine(SzCoreEnvironment& env);
    ~SzCoreEngine() override = default;

    SzCoreEngine(const SzCoreEngine&) = delete;
    SzCoreEngine& operator=(const SzCoreEngine&) = delete;

    void PrimeEngine() override;
    std::string GetStats() override;

    std::string AddRecord(const std::string& dataSourceCode,
                          const std::string& recordID,
                          const std::string& recordDefinition,
                          SzFlags flags = SzAddRecordDefaultFlags) override;
    std::string GetRecordPreview(
        const std::string& recordDefinition,
        SzFlags flags = SzRecordPreviewDefaultFlags) override;
    std::string DeleteRecord(const std::string& dataSourceCode,
                             const std::string& recordID,
                             SzFlags flags = SzDeleteRecordDefaultFlags) override;
    std::string ReevaluateRecord(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzReevaluateRecordDefaultFlags) override;
    std::string ReevaluateEntity(
        int64_t entityID,
        SzFlags flags = SzReevaluateEntityDefaultFlags) override;

    std::string SearchByAttributes(
        const std::string& attributes, const std::string& searchProfile,
        SzFlags flags = SzSearchByAttributesDefaultFlags) override;
    std::string SearchByAttributes(
        const std::string& attributes,
        SzFlags flags = SzSearchByAttributesDefaultFlags) override;
    std::string WhySearch(const std::string& attributes, int64_t entityID,
                          const std::string& searchProfile = "",
                          SzFlags flags = SzWhySearchDefaultFlags) override;

    std::string GetEntity(int64_t entityID,
                          SzFlags flags = SzEntityDefaultFlags) override;
    std::string GetEntity(const std::string& dataSourceCode,
                          const std::string& recordID,
                          SzFlags flags = SzEntityDefaultFlags) override;
    std::string GetRecord(const std::string& dataSourceCode,
                          const std::string& recordID,
                          SzFlags flags = SzRecordDefaultFlags) override;

    std::string FindInterestingEntities(
        int64_t entityID,
        SzFlags flags = SzFindInterestingEntitiesDefaultFlags) override;
    std::string FindInterestingEntities(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzFindInterestingEntitiesDefaultFlags) override;

    std::string FindPath(
        int64_t startEntityID, int64_t endEntityID, int32_t maxDegrees,
        const std::set<int64_t>& avoidEntityIDs = {},
        const std::set<std::string>& requiredDataSources = {},
        SzFlags flags = SzFindPathDefaultFlags) override;
    std::string FindPath(
        const std::string& startDataSourceCode, const std::string& startRecordID,
        const std::string& endDataSourceCode, const std::string& endRecordID,
        int32_t maxDegrees,
        const std::set<std::pair<std::string, std::string>>& avoidRecordKeys = {},
        const std::set<std::string>& requiredDataSources = {},
        SzFlags flags = SzFindPathDefaultFlags) override;

    std::string FindNetwork(
        const std::set<int64_t>& entityIDs, int32_t maxDegrees,
        int32_t buildOutDegrees, int32_t buildOutMaxEntities,
        SzFlags flags = SzFindNetworkDefaultFlags) override;
    std::string FindNetwork(
        const std::set<std::pair<std::string, std::string>>& recordKeys,
        int32_t maxDegrees, int32_t buildOutDegrees, int32_t buildOutMaxEntities,
        SzFlags flags = SzFindNetworkDefaultFlags) override;

    std::string WhyRecordInEntity(
        const std::string& dataSourceCode, const std::string& recordID,
        SzFlags flags = SzWhyRecordInEntityDefaultFlags) override;
    std::string WhyRecords(const std::string& dataSourceCode1,
                           const std::string& recordID1,
                           const std::string& dataSourceCode2,
                           const std::string& recordID2,
                           SzFlags flags = SzWhyRecordsDefaultFlags) override;
    std::string WhyEntities(int64_t entityID1, int64_t entityID2,
                            SzFlags flags = SzWhyEntitiesDefaultFlags) override;
    std::string HowEntity(int64_t entityID,
                          SzFlags flags = SzHowEntityDefaultFlags) override;
    std::string GetVirtualEntity(
        const std::set<std::pair<std::string, std::string>>& recordKeys,
        SzFlags flags = SzVirtualEntityDefaultFlags) override;

    SzExportHandle ExportJsonEntityReport(
        SzFlags flags = SzExportDefaultFlags) override;
    SzExportHandle ExportCsvEntityReport(
        const std::string& csvColumnList,
        SzFlags flags = SzExportDefaultFlags) override;
    std::string FetchNext(SzExportHandle exportHandle) override;
    void CloseExportReport(SzExportHandle exportHandle) override;

    std::string ProcessRedoRecord(const std::string& redoRecord,
                                  SzFlags flags = SzRedoDefaultFlags) override;
    std::string GetRedoRecord() override;
    int64_t CountRedoRecords() override;

    /// Destroys the native engine module. Idempotent; called by the owning
    /// environment during teardown.
    void Destroy();

    /// True once the native module has been destroyed.
    [[nodiscard]] bool IsDestroyed();

    // ---- Internal JSON encode helpers (mirror C# SzCoreEngine encoders) ----
    // Exposed for unit verification; build the arg JSON the native helpers want.

    /// Encodes a set of entity IDs as `{"ENTITIES":[{"ENTITY_ID":n},...]}`.
    static std::string EncodeEntityIDs(const std::set<int64_t>& entityIDs);

    /// Encodes a set of (dataSource, recordID) keys as
    /// `{"RECORDS":[{"DATA_SOURCE":..,"RECORD_ID":..},...]}`.
    static std::string EncodeRecordKeys(
        const std::set<std::pair<std::string, std::string>>& recordKeys);

    /// Encodes a set of data source codes as `{"DATA_SOURCES":[..]}`.
    static std::string EncodeDataSources(
        const std::set<std::string>& dataSources);

private:
    SzCoreEnvironment& env_;
    std::mutex monitor_;
    bool destroyed_{false};
};

}  // namespace senzing::sdk::core

#endif  // SENZING_SDK_CORE_SZCOREENGINE_HPP
