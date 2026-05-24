#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorInteraction.hpp>
#include <AK/EditorUI/EditorPanelModels.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorPropertyEditPhase
    {
        Idle,
        Begun,
        Previewing,
        Committed,
        Cancelled,
        Rejected
    };

    enum class EditorPropertyEditReason
    {
        Unknown,
        TextField,
        NumericField,
        Vec3Field,
        Checkbox,
        Dropdown,
        DragScrub,
        Reset,
        Programmatic
    };

    enum class EditorPropertyValidationCode
    {
        Ok,
        PropertyNotFound,
        ReadOnly,
        TypeMismatch,
        ParseFailed,
        NonFiniteNumber,
        OutOfRange,
        EmptyValue,
        NoActiveEdit,
        AlreadyEditing
    };

    enum class EditorUndoTransactionState
    {
        None,
        Open,
        Previewing,
        Committed,
        Cancelled
    };

    struct EditorPropertyEditPolicy
    {
        bool allowLivePreview = true;
        bool createUndoTransaction = true;
        bool markSceneDirty = true;
        bool allowRangeClamp = true;
        bool requiresFiniteNumbers = true;
        bool allowTypeCoercion = false;
        std::size_t maxTextBytes = 512;
    };

    struct EditorPropertyValidationResult
    {
        bool ok = false;
        bool clamped = false;
        bool valueChanged = false;
        EditorPropertyValidationCode code = EditorPropertyValidationCode::Ok;
        EditorPropertyValue value{};
        std::string message;
    };

    struct EditorUndoTransaction
    {
        u64 id = 0;
        std::string label;
        std::string propertyPath;
        EditorPropertyValue before{};
        EditorPropertyValue after{};
        EditorUndoTransactionState state = EditorUndoTransactionState::None;
        bool sceneDirty = false;
        bool proxyRebuild = false;
        bool undoable = true;
        u64 beginRevision = 0;
        u64 endRevision = 0;
    };

    struct EditorInspectorPropertyEditState
    {
        EditorPropertyEditPhase phase = EditorPropertyEditPhase::Idle;
        EditorPropertyEditReason reason = EditorPropertyEditReason::Unknown;
        std::string propertyPath;
        EditorPropertyValue originalValue{};
        EditorPropertyValue previewValue{};
        EditorPropertyValue committedValue{};
        EditorUndoTransaction activeTransaction{};
        std::vector<EditorUndoTransaction> undoTransactions;
        std::vector<EditorPropertyChangeRequest> committedChanges;
        EditorPropertyEditPolicy policy{};
        std::string lastError;
        u64 nextTransactionId = 1;
        u64 revision = 1;
    };

    struct EditorPropertyCommitResult
    {
        bool consumed = false;
        bool committed = false;
        bool rejected = false;
        bool sceneDirty = false;
        bool modelDirty = false;
        bool proxyRebuild = false;
        EditorPropertyValidationResult validation{};
        EditorPropertyChangeRequest change{};
        EditorUndoTransaction transaction{};
        CommandInvocation command{};
        std::string message;
    };

    struct EditorPropertyEditingDiagnostics
    {
        EditorPropertyEditPhase phase = EditorPropertyEditPhase::Idle;
        std::size_t undoTransactionCount = 0;
        std::size_t committedChangeCount = 0;
        std::size_t dirtyPropertyCount = 0;
        std::size_t rejectedEditCount = 0;
        bool activeEditConsistent = false;
        bool singleUndoTransaction = false;
        bool previewDoesNotCreateUndo = false;
        bool cancelRestoresValue = false;
        bool validationOk = false;
        bool rangeClampOk = false;
        bool proxyRebuildOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorPropertyEditPhase phase);
    const char* ToString(EditorPropertyEditReason reason);
    const char* ToString(EditorPropertyValidationCode code);
    const char* ToString(EditorUndoTransactionState state);

    EditorInspectorPropertyEditState MakeDefaultEditorInspectorPropertyEditState(EditorPropertyEditPolicy policy = {});

    EditorPropertyDesc* FindEditorProperty(EditorPanelModelFrame& frame, std::string_view propertyPath);
    const EditorPropertyDesc* FindEditorProperty(const EditorPanelModelFrame& frame, std::string_view propertyPath);

    bool EditorPropertyValuesEqual(const EditorPropertyValue& a, const EditorPropertyValue& b);
    std::string FormatEditorPropertyValue(const EditorPropertyValue& value);

    EditorPropertyValidationResult ValidateEditorPropertyValue(const EditorPropertyDesc& property, EditorPropertyValue value, const EditorPropertyEditPolicy& policy = {});
    EditorPropertyValidationResult ParseEditorPropertyText(const EditorPropertyDesc& property, std::string_view text, const EditorPropertyEditPolicy& policy = {});

    bool BeginEditorInspectorPropertyEdit(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, std::string propertyPath, EditorPropertyEditReason reason = EditorPropertyEditReason::Programmatic, EditorPropertyEditPolicy policy = {});
    EditorPropertyValidationResult PreviewEditorInspectorPropertyValue(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, EditorPropertyValue value);
    EditorPropertyValidationResult PreviewEditorInspectorPropertyText(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, std::string_view text);
    EditorPropertyCommitResult CommitEditorInspectorPropertyValue(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, EditorPropertyValue value);
    EditorPropertyCommitResult CommitEditorInspectorPropertyText(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame, std::string_view text);
    bool CancelEditorInspectorPropertyEdit(EditorInspectorPropertyEditState& state, EditorPanelModelFrame& frame);

    EditorPropertyChangeRequest BuildEditorPropertyChangeRequest(const EditorPropertyDesc& property, const EditorPropertyValue& oldValue, const EditorPropertyValue& newValue, bool undoable = true);
    bool ApplyEditorPropertyValue(EditorPanelModelFrame& frame, std::string_view propertyPath, const EditorPropertyValue& value, bool markDirty = true);

    EditorPropertyEditingDiagnostics ValidateEditorPropertyEditingState(const EditorInspectorPropertyEditState& state, const EditorPanelModelFrame& frame);
    EditorPropertyEditingDiagnostics RunEditorPropertyEditingDiagnostics();
    std::string FormatEditorUndoTransaction(const EditorUndoTransaction& transaction);
    std::string FormatEditorPropertyValidationResult(const EditorPropertyValidationResult& validation);
    std::string FormatEditorPropertyCommitResult(const EditorPropertyCommitResult& result);
    std::string FormatEditorPropertyEditingDiagnostics(const EditorPropertyEditingDiagnostics& diagnostics);
}
