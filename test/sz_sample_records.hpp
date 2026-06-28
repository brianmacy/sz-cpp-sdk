// sz_sample_records.hpp -- the canonical three sample records shared by the
// engine test suites, built via the SzRecord builder (so the builder is
// exercised) rather than hand-written JSON duplicated across files.
//
// R1 and R2 describe the same person (well-matching) and resolve into one
// entity; R3 is a distinct person. All use the TEST data source (present in the
// default template config).
#ifndef SENZING_SDK_TEST_SZ_SAMPLE_RECORDS_HPP
#define SENZING_SDK_TEST_SZ_SAMPLE_RECORDS_HPP

#include <string>

#include "sz_record.hpp"

namespace senzing::sdk::test {

constexpr const char* kSampleDataSource = "TEST";

inline std::string SampleRecord1() {
    return SzRecord{SzDataSourceCode::Of(kSampleDataSource), SzRecordID::Of("R1"),
                    SzFullName::Of("Robert Smith"), SzDateOfBirth::Of("12/11/1978"),
                    SzFullAddress::Of("123 Main St Las Vegas NV 89132"),
                    SzPhoneNumber::Of("702-919-1300"),
                    SzEmailAddress::Of("bsmith@work.com")}
        .ToString();
}

inline std::string SampleRecord2() {
    return SzRecord{SzDataSourceCode::Of(kSampleDataSource), SzRecordID::Of("R2"),
                    SzFullName::Of("Bob Smith"), SzDateOfBirth::Of("11/12/1978"),
                    SzFullAddress::Of("123 Main Street Las Vegas NV 89132"),
                    SzPhoneNumber::Of("702-919-1300"),
                    SzEmailAddress::Of("bsmith@work.com")}
        .ToString();
}

inline std::string SampleRecord3() {
    return SzRecord{SzDataSourceCode::Of(kSampleDataSource), SzRecordID::Of("R3"),
                    SzFullName::Of("Jane Doe"), SzDateOfBirth::Of("05/05/1985"),
                    SzFullAddress::Of("456 Elm St Reno NV 89501")}
        .ToString();
}

}  // namespace senzing::sdk::test

#endif  // SENZING_SDK_TEST_SZ_SAMPLE_RECORDS_HPP
