// sz_record.hpp -- test-only fluent record builder, ported from the C# test
// helper Senzing.Sdk.Tests.Core.SzRecord (sz-sdk-csharp@4.3.0). Header-only.
//
// Builds a JSON record string from typed components. Single-valued components
// render inline ("NAME_FULL":"..."); repeated components of the same kind render
// as a plural array ("NAMES":[{...},{...}]), exactly as the C# helper does. Used
// by the engine test suites to construct records without hand-writing JSON.
#ifndef SENZING_SDK_TEST_SZ_RECORD_HPP
#define SENZING_SDK_TEST_SZ_RECORD_HPP

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace senzing::sdk::test {

/// A single record-data component. Fragment() returns the inner JSON
/// key/value text (no surrounding braces); PluralName() is non-empty for
/// components that aggregate into a plural array when repeated.
class SzRecordData {
public:
    virtual ~SzRecordData() = default;
    [[nodiscard]] virtual std::string Fragment() const = 0;
    [[nodiscard]] virtual std::string PluralName() const { return ""; }

protected:
    /// JSON-escapes and quotes a value (mirrors C# Utilities.JsonEscape).
    static std::string Quote(const std::string& value) {
        return nlohmann::json(value).dump();
    }
};

using SzRecordDataPtr = std::shared_ptr<SzRecordData>;

class SzDataSourceCode : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::string code) {
        return std::make_shared<SzDataSourceCode>(std::move(code));
    }
    explicit SzDataSourceCode(std::string code) : code_(std::move(code)) {}
    [[nodiscard]] std::string Fragment() const override {
        return "\"DATA_SOURCE\":" + Quote(code_);
    }

private:
    std::string code_;
};

class SzRecordID : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::string id) {
        return std::make_shared<SzRecordID>(std::move(id));
    }
    explicit SzRecordID(std::string id) : id_(std::move(id)) {}
    [[nodiscard]] std::string Fragment() const override {
        return "\"RECORD_ID\":" + Quote(id_);
    }

private:
    std::string id_;
};

class SzRecordType : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::string type) {
        return std::make_shared<SzRecordType>(std::move(type));
    }
    explicit SzRecordType(std::string type) : type_(std::move(type)) {}
    [[nodiscard]] std::string Fragment() const override {
        return "\"RECORD_TYPE\":" + Quote(type_);
    }

private:
    std::string type_;
};

class SzFullName : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::string fullName,
                              std::optional<std::string> nameType = std::nullopt) {
        return std::make_shared<SzFullName>(std::move(fullName),
                                            std::move(nameType));
    }
    SzFullName(std::string fullName, std::optional<std::string> nameType)
        : fullName_(std::move(fullName)), nameType_(std::move(nameType)) {}
    [[nodiscard]] std::string PluralName() const override { return "NAMES"; }
    [[nodiscard]] std::string Fragment() const override {
        std::string out = "\"NAME_FULL\":" + Quote(fullName_);
        if (nameType_.has_value()) {
            out += ",\"NAME_TYPE\":" + Quote(*nameType_);
        }
        return out;
    }

private:
    std::string fullName_;
    std::optional<std::string> nameType_;
};

class SzNameByParts : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::optional<std::string> firstName,
                              std::optional<std::string> lastName,
                              std::optional<std::string> nameType = std::nullopt) {
        return std::make_shared<SzNameByParts>(std::move(firstName),
                                               std::move(lastName),
                                               std::move(nameType));
    }
    SzNameByParts(std::optional<std::string> firstName,
                  std::optional<std::string> lastName,
                  std::optional<std::string> nameType)
        : firstName_(std::move(firstName)),
          lastName_(std::move(lastName)),
          nameType_(std::move(nameType)) {}
    [[nodiscard]] std::string PluralName() const override { return "NAMES"; }
    [[nodiscard]] std::string Fragment() const override {
        std::string out;
        std::string prefix;
        if (firstName_.has_value()) {
            out += "\"NAME_FIRST\":" + Quote(*firstName_);
            prefix = ",";
        }
        if (lastName_.has_value()) {
            out += prefix + "\"NAME_LAST\":" + Quote(*lastName_);
            prefix = ",";
        }
        if (nameType_.has_value()) {
            out += prefix + "\"NAME_TYPE\":" + Quote(*nameType_);
        }
        return out;
    }

private:
    std::optional<std::string> firstName_;
    std::optional<std::string> lastName_;
    std::optional<std::string> nameType_;
};

class SzFullAddress : public SzRecordData {
public:
    static SzRecordDataPtr Of(
        std::string fullAddress,
        std::optional<std::string> addressType = std::nullopt) {
        return std::make_shared<SzFullAddress>(std::move(fullAddress),
                                               std::move(addressType));
    }
    SzFullAddress(std::string fullAddress, std::optional<std::string> addressType)
        : fullAddress_(std::move(fullAddress)),
          addressType_(std::move(addressType)) {}
    [[nodiscard]] std::string PluralName() const override { return "ADDRESSES"; }
    [[nodiscard]] std::string Fragment() const override {
        std::string out = "\"ADDR_FULL\":" + Quote(fullAddress_);
        if (addressType_.has_value()) {
            out += ",\"ADDR_TYPE\":" + Quote(*addressType_);
        }
        return out;
    }

private:
    std::string fullAddress_;
    std::optional<std::string> addressType_;
};

class SzPhoneNumber : public SzRecordData {
public:
    static SzRecordDataPtr Of(
        std::string phone, std::optional<std::string> phoneType = std::nullopt) {
        return std::make_shared<SzPhoneNumber>(std::move(phone),
                                               std::move(phoneType));
    }
    SzPhoneNumber(std::string phone, std::optional<std::string> phoneType)
        : phone_(std::move(phone)), phoneType_(std::move(phoneType)) {}
    [[nodiscard]] std::string PluralName() const override {
        return "PHONE_NUMBERS";
    }
    [[nodiscard]] std::string Fragment() const override {
        std::string out = "\"PHONE_NUMBER\":" + Quote(phone_);
        if (phoneType_.has_value()) {
            out += ",\"PHONE_TYPE\":" + Quote(*phoneType_);
        }
        return out;
    }

private:
    std::string phone_;
    std::optional<std::string> phoneType_;
};

class SzEmailAddress : public SzRecordData {
public:
    static SzRecordDataPtr Of(
        std::string email, std::optional<std::string> emailType = std::nullopt) {
        return std::make_shared<SzEmailAddress>(std::move(email),
                                                std::move(emailType));
    }
    SzEmailAddress(std::string email, std::optional<std::string> emailType)
        : email_(std::move(email)), emailType_(std::move(emailType)) {}
    [[nodiscard]] std::string PluralName() const override { return "EMAILS"; }
    [[nodiscard]] std::string Fragment() const override {
        std::string out = "\"EMAIL_ADDRESS\":" + Quote(email_);
        if (emailType_.has_value()) {
            out += ",\"EMAIL_TYPE\":" + Quote(*emailType_);
        }
        return out;
    }

private:
    std::string email_;
    std::optional<std::string> emailType_;
};

class SzDateOfBirth : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::string date) {
        return std::make_shared<SzDateOfBirth>(std::move(date));
    }
    explicit SzDateOfBirth(std::string date) : date_(std::move(date)) {}
    [[nodiscard]] std::string Fragment() const override {
        return "\"DATE_OF_BIRTH\":" + Quote(date_);
    }

private:
    std::string date_;
};

class SzSocialSecurity : public SzRecordData {
public:
    static SzRecordDataPtr Of(std::string ssn) {
        return std::make_shared<SzSocialSecurity>(std::move(ssn));
    }
    explicit SzSocialSecurity(std::string ssn) : ssn_(std::move(ssn)) {}
    [[nodiscard]] std::string Fragment() const override {
        return "\"SSN_NUMBER\":" + Quote(ssn_);
    }

private:
    std::string ssn_;
};

/// A record builder. Construct with components (the first two being the data
/// source and record ID), then call ToString() for the JSON, or GetRecordKey()
/// for the (dataSource, recordID) pair.
class SzRecord {
public:
    SzRecord(std::initializer_list<SzRecordDataPtr> data) : SzRecord(std::vector<SzRecordDataPtr>(data)) {}

    explicit SzRecord(std::vector<SzRecordDataPtr> data) {
        for (const SzRecordDataPtr& item : data) {
            if (item == nullptr) {
                continue;
            }
            const std::string plural = item->PluralName();
            const std::string key =
                plural.empty() ? ("@" + std::to_string(groups_.size()) + ":" +
                                  item->Fragment())
                               : plural;
            if (auto* group = FindGroup(key)) {
                group->items.push_back(item);
            } else {
                groups_.push_back(Group{key, plural, {item}});
            }
            if (auto* ds = dynamic_cast<SzDataSourceCode*>(item.get())) {
                (void)ds;
                dataSource_ = ExtractValue(item->Fragment());
            }
            if (auto* rid = dynamic_cast<SzRecordID*>(item.get())) {
                (void)rid;
                recordID_ = ExtractValue(item->Fragment());
            }
        }
    }

    [[nodiscard]] std::string ToString() const {
        std::string out = "{";
        std::string prefix;
        for (const Group& group : groups_) {
            out += prefix;
            if (group.items.size() == 1) {
                out += group.items[0]->Fragment();
            } else {
                out += Quote(group.pluralName) + ":[";
                std::string sep;
                for (const SzRecordDataPtr& item : group.items) {
                    out += sep + "{" + item->Fragment() + "}";
                    sep = ",";
                }
                out += "]";
            }
            prefix = ",";
        }
        out += "}";
        return out;
    }

    [[nodiscard]] std::pair<std::string, std::string> GetRecordKey() const {
        return {dataSource_, recordID_};
    }

private:
    struct Group {
        std::string key;
        std::string pluralName;
        std::vector<SzRecordDataPtr> items;
    };

    Group* FindGroup(const std::string& key) {
        for (Group& group : groups_) {
            if (group.key == key) {
                return &group;
            }
        }
        return nullptr;
    }

    static std::string Quote(const std::string& value) {
        return nlohmann::json(value).dump();
    }

    // Extracts the quoted value from a fragment like "DATA_SOURCE":"TEST".
    static std::string ExtractValue(const std::string& fragment) {
        const auto colon = fragment.find(':');
        if (colon == std::string::npos) {
            return "";
        }
        return nlohmann::json::parse(fragment.substr(colon + 1)).get<std::string>();
    }

    std::vector<Group> groups_;
    std::string dataSource_;
    std::string recordID_;
};

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_SZ_RECORD_HPP
