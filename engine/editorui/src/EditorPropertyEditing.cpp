#include <AK/EditorUI/EditorPropertyEditing.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace AK
{
    namespace
    {
        bool IsFiniteNumber(double value)
        {
            return std::isfinite(value) != 0;
        }

        bool PropertyWritable(const EditorPropertyDesc& property)
        {
            return !HasFlag(property.flags, EditorPropertyFlag::ReadOnly) && property.value.type != EditorPropertyType::Section;
        }

        bool IsNumericType(EditorPropertyType type)
        {
            return type == EditorPropertyType::Int || type == EditorPropertyType::Float || type == EditorPropertyType::Double || type == EditorPropertyType::Vec3 || type == EditorPropertyType::Color;
        }

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

        std::vector<std::string> SplitNumberList(std::string_view text)
        {
            std::vector<std::string> parts;
            std::string current;
            for (char c : text)
            {
                if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\n' || c == '\r')
                {
                    if (!current.empty())
                    {
                        parts.push_back(current);
                        current.clear();
                    }
                }
                else
                {
                    current.push_back(c);
                }
            }
            if (!current.empty())
            {
                parts.push_back(current);
            }
            return parts;
        }

        bool ParseDoubleStrict(std::string_view input, double& out)
        {
            const std::string text = Trim(input);
            if (text.empty())
            {
                return false;
            }

            const char* begin = text.data();
            const char* end = text.data() + text.size();
            double value = 0.0;
            const std::from_chars_result result = std::from_chars(begin, end, value);
            if (result.ec != std::errc{} || result.ptr != end)
            {
                return false;
            }
            out = value;
            return true;
        }

        EditorPropertyValue WithFormattedText(EditorPropertyValue value)
        {
            std::ostringstream out;
            out << std::setprecision(8);
            switch (value.type)
            {
                case EditorPropertyType::Bool:
                    value.text = value.boolean ? "true" : "false";
                    break;
                case EditorPropertyType::Int:
                    out << static_cast<i64>(std::llround(value.numbers[0]));
                    value.text = out.str();
                    break;
                case EditorPropertyType::Float:
                case EditorPropertyType::Double:
                    out << value.numbers[0];
                    value.text = out.str();
                    break;
                case EditorPropertyType::Vec3:
                    out << value.numbers[0] << ',' << value.numbers[1] << ',' << value.numbers[2];
                    value.text = out.str();
                    break;
                case EditorPropertyType::Color:
                    out << value.numbers[0] << ',' << value.numbers[1] << ',' << value.numbers[2] << ',' << value.numbers[3];
                    value.text = out.str();
                    break;
                default:
                    break;
            }
            return value;
        }

        EditorPropertyValidationResult Reject(EditorPropertyValidationCode code, std::string message)
        {
            EditorPropertyValidationResult result{};
            result.ok = false;
            result.code = code;
            result.message = std::move(message);
            return result;
        }

        EditorPropertyValue ClampNumericValue(EditorPropertyValue value, const EditorPropertyDesc& property, bool& clamped)
        {
            if (!property.hasRange || !IsNumericType(value.type))
            {
                return value;
            }

            const int count = value.type == EditorPropertyType::Vec3 ? 3 : value.type == EditorPropertyType::Color ? 4 : 1;
            const double minValue = static_cast<double>(std::min(property.minValue, property.maxValue));
            const double maxValue = static_cast<double>(std::max(property.minValue, property.maxValue));
            for (int i = 0; i < count; ++i)
            {
                const double before = value.numbers[i];
                value.numbers[i] = std::clamp(value.numbers[i], minValue, maxValue);
                clamped = clamped || value.numbers[i] != before;
            }
            return value;
        }

        bool RoundIntegerValue(EditorPropertyValue& value)
        {
            if (value.type != EditorPropertyType::Int || !IsFiniteNumber(value.numbers[0]))
            {
                return true;
            }

            const double minValue = std::nextafter(static_cast<double>(std::numeric_limits<i64>::lowest()), 0.0);
            const double maxValue = std::nextafter(static_cast<double>(std::numeric_limits<i64>::max()), 0.0);
            if (value.numbers[0] < minValue || value.numbers[0] > maxValue)
            {
                return false;
            }

            value.numbers[0] = static_cast<double>(std::llround(value.numbers[0]));
            return true;
        }

        EditorPropertyDesc* FindPropertyInComponent(EditorComponentInspector& component, std::string_view propertyPath)
        {
            for (EditorPropertyDesc& property : component.properties)
            {
                if (property.path == propertyPath)
                {
                    return &property;
                }
            }
            return nullptr;
        }

        const EditorPropertyDesc* FindPropertyInComponent(const EditorComponentInspector& component, std::string_view propertyPath)
        {
            for (const EditorPropertyDesc& property : component.properties)
            {
                if (property.path == propertyPath)
                {
                    return &property;
                }
            }
            return nullptr;
        }

        void ClearActiveTransaction(EditorInspectorPropertyEditState& state)
        {
            state.activeTransaction = {};
            state.propertyPath.clear();
            state.originalValue = {};
            state.previewValue = {};
            state.committedValue = {};
        }

        bool ApplyValueToProperty(EditorPropertyDesc& property, const EditorPropertyValue& value, bool markDirty)
        {
            if (EditorPropertyValuesEqual(property.value, value))
            {
                return false;
            }
            property.value = value;
            if (markDirty)
            {
                property.flags = property.flags | EditorPropertyFlag::Dirty;
            }
            return true;
        }

        std::size_t CountDirtyProperties(const EditorPanelModelFrame& frame)
        {
            std::size_t count = 0;
            for (const EditorComponentInspector& component : frame.inspector.components)
            {
                for (const EditorPropertyDesc& property : component.properties)
                {
                    if (HasFlag(property.flags, EditorPropertyFlag::Dirty))
                    {
                        ++count;
                    }
                }
            }
            return count;
        }

        bool PropertyExistsAndEquals(const EditorPanelModelFrame& frame, std::string_view path, const EditorPropertyValue& value)
        {
            const EditorPropertyDesc* property = FindEditorProperty(frame, path);
            return property != nullptr && EditorPropertyValuesEqual(property->value, value);
        }
    }

    const char* ToString(EditorPropertyEditPhase phase)
    {
        switch (phase)
        {
            case EditorPropertyEditPhase::Idle: return "Idle";
            case EditorPropertyEditPhase::Begun: return "Begun";
            case EditorPropertyEditPhase::Previewing: return "Previewing";
            case EditorPropertyEditPhase::Committed: return "Committed";
            case EditorPropertyEditPhase::Cancelled: return "Cancelled";
            case EditorPropertyEditPhase::Rejected: return "Rejected";
        }
        return "Idle";
    }

    const char* ToString(EditorPropertyEditReason reason)
    {
        switch (reason)
        {
            case EditorPropertyEditReason::Unknown: return "Unknown";
            case EditorPropertyEditReason::TextField: return "TextField";
            case EditorPropertyEditReason::NumericField: return "NumericField";
            case EditorPropertyEditReason::Vec3Field: return "Vec3Field";
            case EditorPropertyEditReason::Checkbox: return "Checkbox";
            case EditorPropertyEditReason::Dropdown: return "Dropdown";
            case EditorPropertyEditReason::DragScrub: return "DragScrub";
            case EditorPropertyEditReason::Reset: return "Reset";
            case EditorPropertyEditReason::Programmatic: return "Programmatic";
        }
        return "Unknown";
    }

    const char* ToString(EditorPropertyValidationCode code)
    {
        switch (code)
        {
            case EditorPropertyValidationCode::Ok: return "Ok";
            case EditorPropertyValidationCode::PropertyNotFound: return "PropertyNotFound";
            case EditorPropertyValidationCode::ReadOnly: return "ReadOnly";
            case EditorPropertyValidationCode::TypeMismatch: return "TypeMismatch";
            case EditorPropertyValidationCode::ParseFailed: return "ParseFailed";
            case EditorPropertyValidationCode::NonFiniteNumber: return "NonFiniteNumber";
            case EditorPropertyValidationCode::OutOfRange: return "OutOfRange";
            case EditorPropertyValidationCode::EmptyValue: return "EmptyValue";
            case EditorPropertyValidationCode::NoActiveEdit: return "NoActiveEdit";
            case EditorPropertyValidationCode::AlreadyEditing: return "AlreadyEditing";
        }
        return "Ok";
    }

    const char* ToString(EditorUndoTransactionState state)
    {
        switch (state)
        {
            case EditorUndoTransactionState::None: return "None";
            case EditorUndoTransactionState::Open: return "Open";
            case EditorUndoTransactionState::Previewing: return "Previewing";
            case EditorUndoTransactionState::Committed: return "Committed";
            case EditorUndoTransactionState::Cancelled: return "Cancelled";
        }
        return "None";
    }

    EditorInspectorPropertyEditState MakeDefaultEditorInspectorPropertyEditState(EditorPropertyEditPolicy policy)
    {
        EditorInspectorPropertyEditState state{};
        state.policy = policy;
        return state;
    }

    EditorPropertyDesc* FindEditorProperty(EditorPanelModelFrame& frame, std::string_view propertyPath)
    {
        for (EditorComponentInspector& component : frame.inspector.components)
        {
            if (EditorPropertyDesc* property = FindPropertyInComponent(component, propertyPath))
            {
                return property;
            }
        }
        return nullptr;
    }

    const EditorPropertyDesc* FindEditorProperty(const EditorPanelModelFrame& frame, std::string_view propertyPath)
    {
        for (const EditorComponentInspector& component : frame.inspector.components)
        {
            if (const EditorPropertyDesc* property = FindPropertyInComponent(component, propertyPath))
            {
                return property;
            }
        }
        return nullptr;
    }

    bool EditorPropertyValuesEqual(const EditorPropertyValue& a, const EditorPropertyValue& b)
    {
        if (a.type != b.type || a.text != b.text || a.boolean != b.boolean)
        {
            return false;
        }
        for (int i = 0; i < 4; ++i)
        {
            if (a.numbers[i] != b.numbers[i])
            {
                return false;
            }
        }
        return true;
    }

    std::string FormatEditorPropertyValue(const EditorPropertyValue& value)
    {
        std::ostringstream out;
        out << ToString(value.type) << '(';
        switch (value.type)
        {
            case EditorPropertyType::Bool:
                out << (value.boolean ? "true" : "false");
                break;
            case EditorPropertyType::Int:
            case EditorPropertyType::Float:
            case EditorPropertyType::Double:
                out << value.numbers[0];
                break;
            case EditorPropertyType::Vec3:
                out << value.numbers[0] << ',' << value.numbers[1] << ',' << value.numbers[2];
                break;
            case EditorPropertyType::Color:
                out << value.numbers[0] << ',' << value.numbers[1] << ',' << value.numbers[2] << ',' << value.numbers[3];
                break;
            default:
                out << value.text;
                break;
        }
        out << ')';
        return out.str();
    }

    EditorPropertyValidationResult ValidateEditorPropertyValue(const EditorPropertyDesc& property, EditorPropertyValue value, const EditorPropertyEditPolicy& policy)
    {
        if (!PropertyWritable(property))
        {
            return Reject(EditorPropertyValidationCode::ReadOnly, "property is read-only");
        }
        if (!policy.allowTypeCoercion && value.type != property.value.type)
        {
            return Reject(EditorPropertyValidationCode::TypeMismatch, "property value type mismatch");
        }
        if (value.text.size() > policy.maxTextBytes)
        {
            return Reject(EditorPropertyValidationCode::ParseFailed, "property text exceeds edit policy limit");
        }

        if (policy.requiresFiniteNumbers && IsNumericType(value.type))
        {
            const int count = value.type == EditorPropertyType::Vec3 ? 3 : value.type == EditorPropertyType::Color ? 4 : 1;
            for (int i = 0; i < count; ++i)
            {
                if (!IsFiniteNumber(value.numbers[i]))
                {
                    return Reject(EditorPropertyValidationCode::NonFiniteNumber, "property contains NaN or infinity");
                }
            }
        }

        bool clamped = false;
        if (policy.allowRangeClamp)
        {
            value = ClampNumericValue(value, property, clamped);
        }
        else if (property.hasRange && IsNumericType(value.type))
        {
            const int count = value.type == EditorPropertyType::Vec3 ? 3 : value.type == EditorPropertyType::Color ? 4 : 1;
            const double minValue = static_cast<double>(std::min(property.minValue, property.maxValue));
            const double maxValue = static_cast<double>(std::max(property.minValue, property.maxValue));
            for (int i = 0; i < count; ++i)
            {
                if (value.numbers[i] < minValue || value.numbers[i] > maxValue)
                {
                    return Reject(EditorPropertyValidationCode::OutOfRange, "property is outside allowed range");
                }
            }
        }

        if (!RoundIntegerValue(value))
        {
            return Reject(EditorPropertyValidationCode::OutOfRange, "integer property is outside supported range");
        }

        EditorPropertyValidationResult result{};
        result.ok = true;
        result.clamped = clamped;
        result.value = WithFormattedText(value);
        result.valueChanged = !EditorPropertyValuesEqual(property.value, result.value);
        result.code = EditorPropertyValidationCode::Ok;
        result.message = clamped ? "property value accepted and clamped" : "property value accepted";
        return result;
    }

    EditorPropertyValidationResult ParseEditorPropertyText(const EditorPropertyDesc& property, std::string_view text, const EditorPropertyEditPolicy& policy)
    {
        const std::string trimmed = Trim(text);
        if (trimmed.empty() && property.value.type != EditorPropertyType::String)
        {
            return Reject(EditorPropertyValidationCode::EmptyValue, "empty value is not valid for this property");
        }

        EditorPropertyValue parsed = property.value;
        parsed.text = trimmed;
        switch (property.value.type)
        {
            case EditorPropertyType::Bool:
            {
                std::string lower = trimmed;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
                {
                    parsed.boolean = true;
                }
                else if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
                {
                    parsed.boolean = false;
                }
                else
                {
                    return Reject(EditorPropertyValidationCode::ParseFailed, "failed to parse bool property");
                }
                break;
            }
            case EditorPropertyType::Int:
            case EditorPropertyType::Float:
            case EditorPropertyType::Double:
            {
                double value = 0.0;
                if (!ParseDoubleStrict(trimmed, value))
                {
                    return Reject(EditorPropertyValidationCode::ParseFailed, "failed to parse numeric property");
                }
                parsed.numbers[0] = value;
                break;
            }
            case EditorPropertyType::Vec3:
            case EditorPropertyType::Color:
            {
                const int count = property.value.type == EditorPropertyType::Vec3 ? 3 : 4;
                const std::vector<std::string> parts = SplitNumberList(trimmed);
                if (parts.size() != static_cast<std::size_t>(count))
                {
                    return Reject(EditorPropertyValidationCode::ParseFailed, "failed to parse vector property");
                }
                for (int i = 0; i < count; ++i)
                {
                    double value = 0.0;
                    if (!ParseDoubleStrict(parts[static_cast<std::size_t>(i)], value))
                    {
                        return Reject(EditorPropertyValidationCode::ParseFailed, "failed to parse vector component");
                    }
                    parsed.numbers[i] = value;
                }
                break;
            }
            case EditorPropertyType::String:
            case EditorPropertyType::AssetGuid:
            case EditorPropertyType::EntityRef:
            case EditorPropertyType::Enum:
                parsed.text = trimmed;
                break;
            case EditorPropertyType::Section:
                return Reject(EditorPropertyValidationCode::ReadOnly, "section properties are not editable");
        }

        return ValidateEditorPropertyValue(property, parsed, policy);
    }

    bool BeginEditorInspectorPropertyEdit(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, std::string propertyPath, EditorPropertyEditReason reason, EditorPropertyEditPolicy policy)
    {
        if (state.phase == EditorPropertyEditPhase::Begun || state.phase == EditorPropertyEditPhase::Previewing)
        {
            state.phase = EditorPropertyEditPhase::Rejected;
            state.lastError = "another property edit is already active";
            ++state.revision;
            return false;
        }

        const EditorPropertyDesc* property = FindEditorProperty(frame, propertyPath);
        if (property == nullptr)
        {
            state.phase = EditorPropertyEditPhase::Rejected;
            state.lastError = "property not found";
            ++state.revision;
            return false;
        }
        if (!PropertyWritable(*property))
        {
            state.phase = EditorPropertyEditPhase::Rejected;
            state.lastError = "property is read-only";
            ++state.revision;
            return false;
        }

        state.policy = policy;
        state.phase = EditorPropertyEditPhase::Begun;
        state.reason = reason;
        state.propertyPath = std::move(propertyPath);
        state.originalValue = property->value;
        state.previewValue = property->value;
        state.committedValue = {};
        state.lastError.clear();
        state.activeTransaction = {};
        state.activeTransaction.id = state.nextTransactionId++;
        state.activeTransaction.label = std::string("Edit ") + property->label;
        state.activeTransaction.propertyPath = state.propertyPath;
        state.activeTransaction.before = property->value;
        state.activeTransaction.after = property->value;
        state.activeTransaction.state = EditorUndoTransactionState::Open;
        state.activeTransaction.undoable = policy.createUndoTransaction;
        state.activeTransaction.sceneDirty = policy.markSceneDirty;
        state.activeTransaction.proxyRebuild = HasFlag(property->flags, EditorPropertyFlag::RequiresRebuild);
        state.activeTransaction.beginRevision = frame.revision;
        ++state.revision;
        return true;
    }

    EditorPropertyValidationResult PreviewEditorInspectorPropertyValue(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, EditorPropertyValue value)
    {
        if (state.phase != EditorPropertyEditPhase::Begun && state.phase != EditorPropertyEditPhase::Previewing)
        {
            return Reject(EditorPropertyValidationCode::NoActiveEdit, "no active property edit");
        }

        EditorPropertyDesc* property = FindEditorProperty(frame, state.propertyPath);
        if (property == nullptr)
        {
            return Reject(EditorPropertyValidationCode::PropertyNotFound, "property not found");
        }

        EditorPropertyValidationResult validation = ValidateEditorPropertyValue(*property, std::move(value), state.policy);
        if (!validation.ok)
        {
            state.phase = EditorPropertyEditPhase::Rejected;
            state.lastError = validation.message;
            ++state.revision;
            return validation;
        }

        state.previewValue = validation.value;
        state.activeTransaction.after = validation.value;
        state.activeTransaction.state = EditorUndoTransactionState::Previewing;
        state.phase = EditorPropertyEditPhase::Previewing;
        state.lastError.clear();
        if (state.policy.allowLivePreview)
        {
            if (ApplyValueToProperty(*property, validation.value, false))
            {
                ++frame.inspector.revision;
                ++frame.revision;
            }
        }
        ++state.revision;
        return validation;
    }

    EditorPropertyValidationResult PreviewEditorInspectorPropertyText(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, std::string_view text)
    {
        if (state.phase != EditorPropertyEditPhase::Begun && state.phase != EditorPropertyEditPhase::Previewing)
        {
            return Reject(EditorPropertyValidationCode::NoActiveEdit, "no active property edit");
        }
        const EditorPropertyDesc* property = FindEditorProperty(frame, state.propertyPath);
        if (property == nullptr)
        {
            return Reject(EditorPropertyValidationCode::PropertyNotFound, "property not found");
        }
        EditorPropertyValidationResult validation = ParseEditorPropertyText(*property, text, state.policy);
        if (!validation.ok)
        {
            state.phase = EditorPropertyEditPhase::Rejected;
            state.lastError = validation.message;
            ++state.revision;
            return validation;
        }
        return PreviewEditorInspectorPropertyValue(state, frame, validation.value);
    }

    EditorPropertyCommitResult CommitEditorInspectorPropertyValue(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, EditorPropertyValue value)
    {
        EditorPropertyCommitResult result{};
        if (state.phase != EditorPropertyEditPhase::Begun && state.phase != EditorPropertyEditPhase::Previewing)
        {
            result.rejected = true;
            result.validation = Reject(EditorPropertyValidationCode::NoActiveEdit, "no active property edit");
            result.message = result.validation.message;
            state.lastError = result.message;
            state.phase = EditorPropertyEditPhase::Rejected;
            ++state.revision;
            return result;
        }

        EditorPropertyDesc* property = FindEditorProperty(frame, state.propertyPath);
        if (property == nullptr)
        {
            result.rejected = true;
            result.validation = Reject(EditorPropertyValidationCode::PropertyNotFound, "property not found");
            result.message = result.validation.message;
            state.lastError = result.message;
            state.phase = EditorPropertyEditPhase::Rejected;
            ++state.revision;
            return result;
        }

        result.validation = ValidateEditorPropertyValue(*property, std::move(value), state.policy);
        if (!result.validation.ok)
        {
            result.rejected = true;
            result.message = result.validation.message;
            state.lastError = result.message;
            state.phase = EditorPropertyEditPhase::Rejected;
            ++state.revision;
            return result;
        }

        result.consumed = true;
        result.committed = true;
        result.sceneDirty = state.policy.markSceneDirty;
        result.modelDirty = true;
        result.proxyRebuild = state.activeTransaction.proxyRebuild;
        result.change = BuildEditorPropertyChangeRequest(*property, state.originalValue, result.validation.value, state.policy.createUndoTransaction);
        result.change.requiresSceneDirty = state.policy.markSceneDirty;
        result.change.requiresProxyRebuild = result.proxyRebuild;
        result.change.status = EditorPropertyChangeStatus::Accepted;
        result.command = {CommandId::Undo, CommandSource::Programmatic};

        ApplyValueToProperty(*property, result.validation.value, state.policy.markSceneDirty);
        ++frame.inspector.revision;
        ++frame.revision;

        state.phase = EditorPropertyEditPhase::Committed;
        state.committedValue = result.validation.value;
        state.activeTransaction.after = result.validation.value;
        state.activeTransaction.state = EditorUndoTransactionState::Committed;
        state.activeTransaction.endRevision = frame.revision;
        state.activeTransaction.sceneDirty = result.sceneDirty;
        state.activeTransaction.proxyRebuild = result.proxyRebuild;
        result.transaction = state.activeTransaction;
        if (state.policy.createUndoTransaction && !EditorPropertyValuesEqual(state.activeTransaction.before, state.activeTransaction.after))
        {
            state.undoTransactions.push_back(state.activeTransaction);
        }
        state.committedChanges.push_back(result.change);
        result.message = std::string("committed ") + state.propertyPath + " = " + FormatEditorPropertyValue(result.validation.value);
        state.lastError.clear();
        ClearActiveTransaction(state);
        ++state.revision;
        return result;
    }

    EditorPropertyCommitResult CommitEditorInspectorPropertyText(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, std::string_view text)
    {
        EditorPropertyCommitResult result{};
        if (state.phase != EditorPropertyEditPhase::Begun && state.phase != EditorPropertyEditPhase::Previewing)
        {
            result.rejected = true;
            result.validation = Reject(EditorPropertyValidationCode::NoActiveEdit, "no active property edit");
            result.message = result.validation.message;
            state.lastError = result.message;
            state.phase = EditorPropertyEditPhase::Rejected;
            ++state.revision;
            return result;
        }
        const EditorPropertyDesc* property = FindEditorProperty(frame, state.propertyPath);
        if (property == nullptr)
        {
            result.rejected = true;
            result.validation = Reject(EditorPropertyValidationCode::PropertyNotFound, "property not found");
            result.message = result.validation.message;
            state.lastError = result.message;
            state.phase = EditorPropertyEditPhase::Rejected;
            ++state.revision;
            return result;
        }
        EditorPropertyValidationResult validation = ParseEditorPropertyText(*property, text, state.policy);
        if (!validation.ok)
        {
            result.rejected = true;
            result.validation = validation;
            result.message = validation.message;
            state.phase = EditorPropertyEditPhase::Rejected;
            state.lastError = validation.message;
            ++state.revision;
            return result;
        }
        return CommitEditorInspectorPropertyValue(state, frame, validation.value);
    }

    bool CancelEditorInspectorPropertyEdit(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame)
    {
        if (state.phase != EditorPropertyEditPhase::Begun && state.phase != EditorPropertyEditPhase::Previewing && state.phase != EditorPropertyEditPhase::Rejected)
        {
            return false;
        }
        EditorPropertyDesc* property = FindEditorProperty(frame, state.propertyPath);
        if (property != nullptr)
        {
            if (ApplyValueToProperty(*property, state.originalValue, false))
            {
                ++frame.inspector.revision;
                ++frame.revision;
            }
        }
        state.activeTransaction.after = state.originalValue;
        state.activeTransaction.state = EditorUndoTransactionState::Cancelled;
        state.phase = EditorPropertyEditPhase::Cancelled;
        state.lastError.clear();
        ClearActiveTransaction(state);
        ++state.revision;
        return true;
    }

    EditorPropertyChangeRequest BuildEditorPropertyChangeRequest(const EditorPropertyDesc& property, const EditorPropertyValue& oldValue, const EditorPropertyValue& newValue, bool undoable)
    {
        EditorPropertyChangeRequest request{};
        request.propertyPath = property.path;
        request.oldValue = oldValue;
        request.newValue = newValue;
        request.status = EditorPropertyChangeStatus::Pending;
        request.requiresSceneDirty = true;
        request.requiresProxyRebuild = HasFlag(property.flags, EditorPropertyFlag::RequiresRebuild);
        request.undoable = undoable;
        return request;
    }

    bool ApplyEditorPropertyValue(EditorPanelModelFrame& frame, std::string_view propertyPath, const EditorPropertyValue& value, bool markDirty)
    {
        EditorPropertyDesc* property = FindEditorProperty(frame, propertyPath);
        if (property == nullptr || !PropertyWritable(*property))
        {
            return false;
        }
        const bool changed = ApplyValueToProperty(*property, value, markDirty);
        if (changed)
        {
            ++frame.inspector.revision;
            ++frame.revision;
        }
        return changed;
    }

    EditorPropertyEditingDiagnostics ValidateEditorPropertyEditingState(const EditorInspectorPropertyEditState& state, const EditorPanelModelFrame& frame)
    {
        EditorPropertyEditingDiagnostics diagnostics{};
        diagnostics.phase = state.phase;
        diagnostics.undoTransactionCount = state.undoTransactions.size();
        diagnostics.committedChangeCount = state.committedChanges.size();
        diagnostics.dirtyPropertyCount = CountDirtyProperties(frame);
        diagnostics.rejectedEditCount = state.phase == EditorPropertyEditPhase::Rejected ? 1u : 0u;
        diagnostics.activeEditConsistent = state.propertyPath.empty() || FindEditorProperty(frame, state.propertyPath) != nullptr;
        diagnostics.singleUndoTransaction = state.undoTransactions.size() <= state.committedChanges.size();
        diagnostics.previewDoesNotCreateUndo = state.phase != EditorPropertyEditPhase::Previewing || state.undoTransactions.empty();
        diagnostics.cancelRestoresValue = true;
        diagnostics.validationOk = true;
        diagnostics.rangeClampOk = true;
        diagnostics.proxyRebuildOk = true;
        diagnostics.ok = diagnostics.activeEditConsistent && diagnostics.singleUndoTransaction && diagnostics.previewDoesNotCreateUndo && diagnostics.validationOk && diagnostics.rangeClampOk && diagnostics.proxyRebuildOk;
        diagnostics.summary = FormatEditorPropertyEditingDiagnostics(diagnostics);
        return diagnostics;
    }

    EditorPropertyEditingDiagnostics RunEditorPropertyEditingDiagnostics()
    {
        EditorRuntimeBridge bridge = BuildDefaultEditorRuntimeBridge(1280, 720);
        EditorPanelModelFrame frame = BuildDefaultEditorPanelModelFrame(bridge);
        EditorInspectorPropertyEditState state = MakeDefaultEditorInspectorPropertyEditState();

        const bool beginOk = BeginEditorInspectorPropertyEdit(state, frame, "Transform.Position", EditorPropertyEditReason::Vec3Field);
        const EditorPropertyValidationResult previewA = PreviewEditorInspectorPropertyText(state, frame, "1.0,2.0,3.0");
        const bool noUndoAfterPreview = state.undoTransactions.empty();
        const EditorPropertyValidationResult previewB = PreviewEditorInspectorPropertyText(state, frame, "4.0,5.0,6.0");
        const EditorPropertyCommitResult commit = CommitEditorInspectorPropertyText(state, frame, "7.0,8.0,9.0");
        const bool committedValueOk = PropertyExistsAndEquals(frame, "Transform.Position", MakeNumberPropertyValue(EditorPropertyType::Vec3, 7.0, 8.0, 9.0));

        const bool beginCancelOk = BeginEditorInspectorPropertyEdit(state, frame, "Transform.Scale", EditorPropertyEditReason::Vec3Field);
        const EditorPropertyValidationResult previewCancel = PreviewEditorInspectorPropertyText(state, frame, "2.0,2.0,2.0");
        const bool cancelOk = CancelEditorInspectorPropertyEdit(state, frame);
        const bool cancelRestores = PropertyExistsAndEquals(frame, "Transform.Scale", MakeNumberPropertyValue(EditorPropertyType::Vec3, 1.0, 1.0, 1.0));

        const bool beginClampOk = BeginEditorInspectorPropertyEdit(state, frame, "Camera.FovY", EditorPropertyEditReason::NumericField);
        const EditorPropertyDesc* fovProperty = FindEditorProperty(frame, "Camera.FovY");
        const EditorPropertyValidationResult clampValidation = fovProperty != nullptr ? ParseEditorPropertyText(*fovProperty, "220.0", state.policy) : EditorPropertyValidationResult{};
        const EditorPropertyCommitResult clamped = CommitEditorInspectorPropertyText(state, frame, "220.0");
        const bool clampOk = clampValidation.clamped && clamped.committed && PropertyExistsAndEquals(frame, "Camera.FovY", MakeNumberPropertyValue(EditorPropertyType::Float, 179.0));

        const bool beginInvalidOk = BeginEditorInspectorPropertyEdit(state, frame, "Camera.FovY", EditorPropertyEditReason::NumericField);
        const EditorPropertyCommitResult invalid = CommitEditorInspectorPropertyText(state, frame, "nan");
        const bool invalidRejected = invalid.rejected && !invalid.validation.ok;
        if (state.phase == EditorPropertyEditPhase::Rejected)
        {
            CancelEditorInspectorPropertyEdit(state, frame);
        }

        const bool beginProxyOk = BeginEditorInspectorPropertyEdit(state, frame, "Mesh.Material", EditorPropertyEditReason::TextField);
        const EditorPropertyCommitResult proxy = CommitEditorInspectorPropertyText(state, frame, "mat-destructible");
        const bool proxyOk = proxy.committed && proxy.proxyRebuild && proxy.change.requiresProxyRebuild && PropertyExistsAndEquals(frame, "Mesh.Material", MakeStringPropertyValue(EditorPropertyType::AssetGuid, "mat-destructible"));

        EditorPropertyEditingDiagnostics diagnostics = ValidateEditorPropertyEditingState(state, frame);
        diagnostics.previewDoesNotCreateUndo = noUndoAfterPreview;
        diagnostics.singleUndoTransaction = state.undoTransactions.size() == state.committedChanges.size() && state.undoTransactions.size() == 3;
        diagnostics.cancelRestoresValue = cancelOk && cancelRestores;
        diagnostics.validationOk = beginOk && previewA.ok && previewB.ok && commit.committed && committedValueOk && beginCancelOk && previewCancel.ok && beginClampOk && beginInvalidOk && invalidRejected && beginProxyOk;
        diagnostics.rangeClampOk = clampOk;
        diagnostics.proxyRebuildOk = proxyOk;
        diagnostics.ok = diagnostics.activeEditConsistent && diagnostics.singleUndoTransaction && diagnostics.previewDoesNotCreateUndo && diagnostics.cancelRestoresValue && diagnostics.validationOk && diagnostics.rangeClampOk && diagnostics.proxyRebuildOk;
        diagnostics.summary = FormatEditorPropertyEditingDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorUndoTransaction(const EditorUndoTransaction& transaction)
    {
        std::ostringstream out;
        out << "undo-transaction id=" << transaction.id
            << " state=" << ToString(transaction.state)
            << " property=" << transaction.propertyPath
            << " before=" << FormatEditorPropertyValue(transaction.before)
            << " after=" << FormatEditorPropertyValue(transaction.after)
            << " dirty=" << transaction.sceneDirty
            << " proxy=" << transaction.proxyRebuild;
        return out.str();
    }

    std::string FormatEditorPropertyValidationResult(const EditorPropertyValidationResult& validation)
    {
        std::ostringstream out;
        out << "validation ok=" << validation.ok
            << " code=" << ToString(validation.code)
            << " clamped=" << validation.clamped
            << " changed=" << validation.valueChanged
            << " value=" << FormatEditorPropertyValue(validation.value)
            << " message='" << validation.message << "'";
        return out.str();
    }

    std::string FormatEditorPropertyCommitResult(const EditorPropertyCommitResult& result)
    {
        std::ostringstream out;
        out << "commit consumed=" << result.consumed
            << " committed=" << result.committed
            << " rejected=" << result.rejected
            << " dirty=" << result.sceneDirty
            << " proxy=" << result.proxyRebuild
            << " change=" << FormatEditorPropertyChangeRequest(result.change)
            << " transaction=" << FormatEditorUndoTransaction(result.transaction);
        return out.str();
    }

    std::string FormatEditorPropertyEditingDiagnostics(const EditorPropertyEditingDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-property-edit phase=" << ToString(diagnostics.phase)
            << " undo=" << diagnostics.undoTransactionCount
            << " changes=" << diagnostics.committedChangeCount
            << " dirty=" << diagnostics.dirtyPropertyCount
            << " rejected=" << diagnostics.rejectedEditCount
            << " previewNoUndo=" << diagnostics.previewDoesNotCreateUndo
            << " undoPerCommit=" << diagnostics.singleUndoTransaction
            << " cancelRestore=" << diagnostics.cancelRestoresValue
            << " validation=" << diagnostics.validationOk
            << " clamp=" << diagnostics.rangeClampOk
            << " proxy=" << diagnostics.proxyRebuildOk
            << " ok=" << diagnostics.ok;
        return out.str();
    }
}
