#include <AK/Serialization/Schema.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        std::string Trim(std::string_view text)
        {
            std::size_t begin = 0;
            std::size_t end = text.size();
            while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
            {
                ++begin;
            }
            while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            {
                --end;
            }
            return std::string(text.substr(begin, end - begin));
        }

        bool ParseU32Strict(std::string_view text, u32& value)
        {
            const std::string trimmed = Trim(text);
            if (trimmed.empty())
            {
                return false;
            }
            const char* begin = trimmed.data();
            const char* end = begin + trimmed.size();
            u32 parsed = 0;
            const auto result = std::from_chars(begin, end, parsed);
            if (result.ec != std::errc{} || result.ptr != end)
            {
                return false;
            }
            value = parsed;
            return true;
        }

        std::vector<std::string_view> SplitLines(std::string_view text)
        {
            std::vector<std::string_view> lines;
            std::size_t start = 0;
            while (start <= text.size())
            {
                const std::size_t end = text.find('\n', start);
                if (end == std::string_view::npos)
                {
                    std::string_view line = text.substr(start);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    lines.push_back(line);
                    break;
                }

                std::string_view line = text.substr(start, end - start);
                if (!line.empty() && line.back() == '\r')
                {
                    line.remove_suffix(1);
                }
                lines.push_back(line);
                start = end + 1;
            }
            return lines;
        }

        bool NeedsQuoting(std::string_view text)
        {
            if (text.empty())
            {
                return true;
            }

            for (char c : text)
            {
                const unsigned char ch = static_cast<unsigned char>(c);
                if (std::isspace(ch) != 0 || c == '"' || c == '\\' || c == '=' || c == '#')
                {
                    return true;
                }
            }
            return false;
        }

        void AppendMigration(VersionedSchema& schema, u32 from, u32 to, std::string name)
        {
            schema.migrations.push_back({from, to, std::move(name)});
        }
    }

    Result<VersionedDocumentHeader> ParseVersionedDocumentHeader(std::string_view text)
    {
        for (std::string_view rawLine : SplitLines(text))
        {
            const std::string line = Trim(rawLine);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            std::istringstream stream(line);
            std::string magic;
            std::string versionText;
            std::string extra;
            stream >> magic >> versionText >> extra;
            if (magic.empty() || versionText.empty() || !extra.empty())
            {
                return Result<VersionedDocumentHeader>(MakeError(ErrorCode::ParseError, "expected versioned header: <MAGIC> <VERSION>"));
            }

            u32 version = 0;
            if (!ParseU32Strict(versionText, version) || version == 0)
            {
                return Result<VersionedDocumentHeader>(MakeError(ErrorCode::ParseError, "header version must be a positive integer"));
            }

            return VersionedDocumentHeader{magic, version, true};
        }

        return Result<VersionedDocumentHeader>(MakeError(ErrorCode::ParseError, "document is empty or contains only comments"));
    }

    const char* ToString(SchemaCompatibility compatibility)
    {
        switch (compatibility)
        {
        case SchemaCompatibility::Compatible:
            return "compatible";
        case SchemaCompatibility::NeedsMigration:
            return "needs-migration";
        case SchemaCompatibility::TooOld:
            return "too-old";
        case SchemaCompatibility::TooNew:
            return "too-new";
        case SchemaCompatibility::Invalid:
        default:
            return "invalid";
        }
    }

    std::string ToDebugString(const VersionedDocumentHeader& header)
    {
        std::ostringstream out;
        out << "Header: " << (header.valid ? header.magic : "<invalid>") << " version=" << header.version;
        return out.str();
    }

    std::string ToDebugString(const VersionedSchema& schema)
    {
        std::ostringstream out;
        out << "Schema " << schema.magic
            << " current=" << schema.currentVersion
            << " readable=" << schema.minimumReadableVersion
            << " writable=" << schema.minimumWritableVersion
            << " migrations=" << schema.migrations.size();
        return out.str();
    }

    std::string ToDebugString(const SchemaMigrationPlan& plan)
    {
        std::ostringstream out;
        out << "Migration plan: " << plan.magic
            << " " << plan.sourceVersion << "->" << plan.targetVersion
            << " " << ToString(plan.compatibility)
            << " steps=" << plan.steps.size()
            << " warnings=" << plan.warnings.size();
        return out.str();
    }

    std::string ToDebugString(const KeyValueDocument& document)
    {
        std::ostringstream out;
        out << "KeyValue document: " << document.headerMagic
            << " version=" << document.headerVersion
            << " keys=" << document.values.size()
            << " warnings=" << document.warnings.size();
        return out.str();
    }

    SchemaMigrationPlan BuildSchemaMigrationPlan(const VersionedSchema& schema, const VersionedDocumentHeader& header)
    {
        SchemaMigrationPlan plan{};
        plan.magic = schema.magic;
        plan.sourceVersion = header.version;
        plan.targetVersion = schema.currentVersion;

        if (!header.valid || header.magic != schema.magic || schema.magic.empty() || schema.currentVersion == 0)
        {
            plan.compatibility = SchemaCompatibility::Invalid;
            plan.warnings.push_back("document magic does not match schema or schema is invalid");
        }
        else if (header.version < schema.minimumReadableVersion)
        {
            plan.compatibility = SchemaCompatibility::TooOld;
            plan.warnings.push_back("document is older than minimum readable version");
        }
        else if (header.version > schema.currentVersion)
        {
            plan.compatibility = SchemaCompatibility::TooNew;
            plan.warnings.push_back("document was written by a newer engine version");
        }
        else if (header.version == schema.currentVersion)
        {
            plan.compatibility = SchemaCompatibility::Compatible;
        }
        else
        {
            plan.compatibility = SchemaCompatibility::NeedsMigration;
            u32 cursor = header.version;
            while (cursor < schema.currentVersion)
            {
                const auto it = std::find_if(schema.migrations.begin(), schema.migrations.end(), [cursor](const SchemaMigrationStep& step)
                {
                    return step.fromVersion == cursor && step.toVersion > cursor;
                });

                if (it == schema.migrations.end())
                {
                    plan.compatibility = SchemaCompatibility::Invalid;
                    plan.warnings.push_back("missing migration step from version " + std::to_string(cursor));
                    break;
                }

                plan.steps.push_back(*it);
                cursor = it->toVersion;
            }
        }

        plan.summary = ToDebugString(plan);
        return plan;
    }

    VersionedSchema BuildSceneSchemaV6()
    {
        VersionedSchema schema{};
        schema.magic = "AKSCENE";
        schema.currentVersion = 6;
        schema.minimumReadableVersion = 1;
        schema.minimumWritableVersion = 6;
        AppendMigration(schema, 1, 2, "quoted entity names");
        AppendMigration(schema, 2, 3, "mesh camera light components");
        AppendMigration(schema, 3, 4, "large world coordinates");
        AppendMigration(schema, 4, 5, "generational entity ids");
        AppendMigration(schema, 5, 6, "bounds components");
        return schema;
    }

    VersionedSchema BuildProjectSettingsSchemaV1()
    {
        return {"AKSETTINGS", 1, 1, 1, {}};
    }

    VersionedSchema BuildAssetDatabaseSchemaV1()
    {
        return {"AKASSETDB", 1, 1, 1, {}};
    }

    VersionedSchema BuildAssetPackageSchemaV1()
    {
        return {"AKPAK", 1, 1, 1, {}};
    }

    std::string EscapeTextValue(std::string_view text)
    {
        std::string result;
        result.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result.push_back(c);
                break;
            }
        }
        return result;
    }

    Result<std::string> UnescapeTextValue(std::string_view text)
    {
        std::string result;
        result.reserve(text.size());
        bool escaping = false;
        for (char c : text)
        {
            if (!escaping)
            {
                if (c == '\\')
                {
                    escaping = true;
                }
                else
                {
                    result.push_back(c);
                }
                continue;
            }

            switch (c)
            {
            case '\\':
                result.push_back('\\');
                break;
            case '"':
                result.push_back('"');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            default:
                return Result<std::string>(MakeError(ErrorCode::ParseError, "unknown escape sequence"));
            }
            escaping = false;
        }

        if (escaping)
        {
            return Result<std::string>(MakeError(ErrorCode::ParseError, "unfinished escape sequence"));
        }
        return result;
    }

    std::string QuoteTextValue(std::string_view text)
    {
        if (!NeedsQuoting(text))
        {
            return std::string(text);
        }
        return "\"" + EscapeTextValue(text) + "\"";
    }

    Result<std::string> UnquoteTextValue(std::string_view text)
    {
        const std::string trimmed = Trim(text);
        if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"')
        {
            return UnescapeTextValue(std::string_view(trimmed).substr(1, trimmed.size() - 2));
        }
        if (trimmed.find('"') != std::string::npos)
        {
            return Result<std::string>(MakeError(ErrorCode::ParseError, "unquoted value contains a quote"));
        }
        return trimmed;
    }

    Result<KeyValueDocument> ParseKeyValueDocument(std::string_view text, bool requireHeader)
    {
        KeyValueDocument document{};
        bool headerSeen = false;
        u32 lineNumber = 0;

        for (std::string_view rawLine : SplitLines(text))
        {
            ++lineNumber;
            const std::string line = Trim(rawLine);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            if (!headerSeen)
            {
                const Result<VersionedDocumentHeader> header = ParseVersionedDocumentHeader(line);
                if (header)
                {
                    document.headerMagic = header.Value().magic;
                    document.headerVersion = header.Value().version;
                    headerSeen = true;
                    continue;
                }

                if (requireHeader)
                {
                    return Result<KeyValueDocument>(MakeError(ErrorCode::ParseError, "missing required key-value document header"));
                }
                headerSeen = true;
            }

            const std::size_t equals = line.find('=');
            if (equals == std::string::npos)
            {
                document.warnings.push_back("line " + std::to_string(lineNumber) + ": missing '=' ignored");
                continue;
            }

            const std::string key = Trim(std::string_view(line).substr(0, equals));
            const std::string valueText = Trim(std::string_view(line).substr(equals + 1));
            if (key.empty())
            {
                document.warnings.push_back("line " + std::to_string(lineNumber) + ": empty key ignored");
                continue;
            }

            Result<std::string> value = UnquoteTextValue(valueText);
            if (!value)
            {
                return Result<KeyValueDocument>(MakeError(value.GetError().code, "line " + std::to_string(lineNumber) + ": " + value.GetError().message));
            }

            const auto [it, inserted] = document.values.emplace(key, value.Value());
            if (!inserted)
            {
                it->second = value.Value();
                document.warnings.push_back("line " + std::to_string(lineNumber) + ": duplicate key overwritten: " + key);
            }
        }

        if (requireHeader && document.headerMagic.empty())
        {
            return Result<KeyValueDocument>(MakeError(ErrorCode::ParseError, "missing required key-value document header"));
        }
        return document;
    }

    std::string SerializeKeyValueDocument(const KeyValueDocument& document)
    {
        std::ostringstream out;
        if (!document.headerMagic.empty() && document.headerVersion > 0)
        {
            out << document.headerMagic << ' ' << document.headerVersion << '\n';
        }

        for (const auto& [key, value] : document.values)
        {
            out << key << '=' << QuoteTextValue(value) << '\n';
        }
        return out.str();
    }

    SerializationProbeResult BuildSerializationProbe()
    {
        SerializationProbeResult probe{};
        const std::string sceneText = "AKSCENE 2\nname=\"Old Sandbox\"\nentity=\"Sample Cube\"\n";
        const Result<VersionedDocumentHeader> header = ParseVersionedDocumentHeader(sceneText);
        if (header)
        {
            probe.header = header.Value();
            probe.scenePlan = BuildSchemaMigrationPlan(BuildSceneSchemaV6(), probe.header);
        }

        const std::string keyValueText = "AKTEST 1\nprojectName=\"AK Sandbox\"\npath=assets\\\\mesh cube.glb\nmode=development\nmode=release\n";
        const Result<KeyValueDocument> keyValues = ParseKeyValueDocument(keyValueText, true);
        if (keyValues)
        {
            probe.keyValues = keyValues.Value();
            probe.serialized = SerializeKeyValueDocument(probe.keyValues);
        }

        probe.escaped = QuoteTextValue("mesh cube \"hero\".glb");
        const Result<std::string> unescaped = UnquoteTextValue(probe.escaped);
        if (unescaped)
        {
            probe.unescaped = unescaped.Value();
        }

        probe.ok = header.Ok()
            && keyValues.Ok()
            && unescaped.Ok()
            && probe.header.magic == "AKSCENE"
            && probe.header.version == 2
            && probe.scenePlan.compatibility == SchemaCompatibility::NeedsMigration
            && probe.scenePlan.steps.size() == 4
            && probe.keyValues.values.size() == 3
            && probe.keyValues.values.at("mode") == "release"
            && probe.unescaped == "mesh cube \"hero\".glb";

        std::ostringstream summary;
        summary << "Serialization probe: " << (probe.ok ? "ok" : "failed")
                << " schema=" << probe.header.magic
                << " source=" << probe.scenePlan.sourceVersion
                << " target=" << probe.scenePlan.targetVersion
                << " steps=" << probe.scenePlan.steps.size()
                << " kv=" << probe.keyValues.values.size()
                << " warnings=" << probe.keyValues.warnings.size();
        probe.summary = summary.str();
        return probe;
    }

    std::string BuildSerializationProbeSummary()
    {
        return BuildSerializationProbe().summary;
    }
}
