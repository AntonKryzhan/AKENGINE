#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/EditorUI/EditorUX.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorUXPopupCloseReason
    {
        None,
        Escape,
        OutsideClick,
        CommandAccepted,
        Replaced
    };

    struct EditorUXPopupItemRuntime
    {
        std::string id;
        std::string label;
        EditorIconKind icon = EditorIconKind::None;
        EditorRect rect{};
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
        bool separatorBefore = false;
        bool hasSubmenu = false;
        CommandId command = CommandId::Count;
    };

    struct EditorUXPopupSurface
    {
        std::string id;
        std::string title;
        EditorPopupAnchorKind anchorKind = EditorPopupAnchorKind::None;
        EditorRect rect{};
        EditorRect anchor{};
        bool open = false;
        bool popupPanel = false;
        std::vector<EditorUXPopupItemRuntime> items;
    };

    struct EditorUXRuntime
    {
        bool popupOpen = false;
        EditorPopupWorkflowDesc activeWorkflow{};
        EditorUXPopupSurface surface{};
        std::string hoveredItemId;
        std::string lastAcceptedItemId;
        EditorUXPopupCloseReason lastCloseReason = EditorUXPopupCloseReason::None;
        u64 revision = 1;
    };

    struct EditorUXHitResult
    {
        bool insidePopup = false;
        bool insideItem = false;
        std::string itemId;
        CommandId command = CommandId::Count;
        bool enabled = false;
    };

    struct EditorUXActivationResult
    {
        bool consumed = false;
        bool closePopup = false;
        std::string itemId;
        CommandInvocation invocation{};
        std::string message;
    };

    struct EditorUXRuntimeDiagnostics
    {
        bool popupOpen = false;
        std::size_t surfaceItemCount = 0;
        std::size_t commandItemCount = 0;
        std::size_t disabledItemCount = 0;
        std::size_t destructiveItemCount = 0;
        std::size_t invalidRectCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorUXPopupCloseReason reason);

    EditorRect MakeEditorRect(i32 x, i32 y, i32 width, i32 height);
    bool EditorRectContains(EditorRect rect, i32 x, i32 y);
    bool OpenEditorUXPopup(EditorUXRuntime& runtime, EditorPopupWorkflowDesc workflow, i32 viewportWidth, i32 viewportHeight);
    bool CloseEditorUXPopup(EditorUXRuntime& runtime, EditorUXPopupCloseReason reason);
    EditorUXHitResult HitTestEditorUXPopup(EditorUXRuntime& runtime, i32 x, i32 y);
    EditorUXActivationResult ActivateEditorUXPopupItem(EditorUXRuntime& runtime, i32 x, i32 y);
    EditorUXPopupSurface BuildEditorUXPopupSurface(const EditorPopupWorkflowDesc& workflow, i32 viewportWidth, i32 viewportHeight);
    EditorUXRuntimeDiagnostics ValidateEditorUXRuntime(const EditorUXRuntime& runtime);
    std::string FormatEditorUXPopupSurface(const EditorUXPopupSurface& surface);
    std::string FormatEditorUXRuntimeDiagnostics(const EditorUXRuntimeDiagnostics& diagnostics);
}
