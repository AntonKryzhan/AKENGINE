#pragma once

#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class SchemaCompatibility
    {
        Compatible,
        NeedsMigration,
        TooOld,
        TooNew,
        Invalid
    };

    struct VersionedDocumentHeader final
    {
        std::string magic;
        u32 version = 0;
        bool valid = false;
    };

    struct SchemaMigrationStep final
    {
        u32 fromVersion = 0;
        u32 toVersion = 0;
        std::string name;
    };

    struct VersionedSchema final
    {
        std::string magic;
        u32 currentVersion = 1;
        u32 minimumReadableVersion = 1;
        u32 minimumWritableVersion = 1;
        std::vector<SchemaMigrationStep> migrations;
    };

    struct SchemaMigrationPlan final
    {
        std::string magic;
        u32 sourceVersion = 0;
        u32 targetVersion = 0;
        SchemaCompatibility compatibility = SchemaCompatibility::Invalid;
        std::vector<SchemaMigrationStep> steps;
        std::vector<std::string> warnings;
        std::string summary;
    };

    struct KeyValueDocument final
    {
        std::string headerMagic;
        u32 headerVersion = 0;
        std::map<std::string, std::string> values;
        std::vector<std::string> warnings;
    };

    struct SerializationProbeResult final
    {
        bool ok = false;
        VersionedDocumentHeader header{};
        SchemaMigrationPlan scenePlan{};
        KeyValueDocument keyValues{};
        std::string escaped;
        std::string unescaped;
        std::string serialized;
        std::string summary;
    };

    Result<VersionedDocumentHeader> ParseVersionedDocumentHeader(std::string_view text);

    const char* ToString(SchemaCompatibility compatibility);
    std::string ToDebugString(const VersionedDocumentHeader& header);
    std::string ToDebugString(const VersionedSchema& schema);
    std::string ToDebugString(const SchemaMigrationPlan& plan);
    std::string ToDebugString(const KeyValueDocument& document);

    SchemaMigrationPlan BuildSchemaMigrationPlan(const VersionedSchema& schema, const VersionedDocumentHeader& header);
    VersionedSchema BuildSceneSchemaV6();
    VersionedSchema BuildProjectSettingsSchemaV1();
    VersionedSchema BuildAssetDatabaseSchemaV1();
    VersionedSchema BuildAssetPackageSchemaV1();

    std::string EscapeTextValue(std::string_view text);
    Result<std::string> UnescapeTextValue(std::string_view text);
    std::string QuoteTextValue(std::string_view text);
    Result<std::string> UnquoteTextValue(std::string_view text);

    Result<KeyValueDocument> ParseKeyValueDocument(std::string_view text, bool requireHeader);
    std::string SerializeKeyValueDocument(const KeyValueDocument& document);

    SerializationProbeResult BuildSerializationProbe();
    std::string BuildSerializationProbeSummary();
}
