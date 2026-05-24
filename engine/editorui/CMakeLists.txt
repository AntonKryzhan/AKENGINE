add_library(ak_editorui STATIC
    src/EditorPanel.cpp
    src/EditorDock.cpp
    src/EditorChrome.cpp
    src/EditorSelection.cpp
    src/EditorSearch.cpp
    src/EditorRuntimeBridge.cpp
    src/EditorPanelModels.cpp
    src/EditorInteraction.cpp
    src/EditorInteractionProbe.cpp
    src/EditorPanelProbe.cpp
    src/EditorUIProbe.cpp
    src/EditorRuntimeProbe.cpp
    src/EditorWidgets.cpp
    src/EditorWidgetProbe.cpp
    src/EditorUX.cpp
    src/EditorUXProbe.cpp
    src/EditorUXRuntime.cpp
    src/EditorUXRuntimeProbe.cpp
    src/EditorText.cpp
    src/EditorScrollClip.cpp
    src/EditorFocus.cpp
    src/EditorFocusProbe.cpp
    src/EditorDragDrop.cpp
    src/EditorDragDropProbe.cpp
    src/EditorPropertyEditing.cpp
    src/EditorPropertyEditingProbe.cpp
    src/EditorCommandState.cpp
    src/EditorCommandStateProbe.cpp
    src/EditorTheme.cpp
    src/EditorThemeProbe.cpp
    src/EditorLayoutPersistence.cpp
    src/EditorLayoutPersistenceProbe.cpp
    src/EditorSessionState.cpp
    src/EditorSessionStateProbe.cpp
    src/EditorShortcuts.cpp
    src/EditorShortcutProbe.cpp
    src/EditorPreferences.cpp
    src/EditorPreferencesProbe.cpp
    src/EditorWorkspace.cpp
    src/EditorWorkspaceProbe.cpp
    src/EditorAutosave.cpp
    src/EditorAutosaveProbe.cpp
    src/EditorActivity.cpp
    src/EditorActivityProbe.cpp
    src/EditorDiagnosticsPanel.cpp
    src/EditorDiagnosticsPanelProbe.cpp
    src/EditorDiagnosticsView.cpp
    src/EditorDiagnosticsInteraction.cpp
    src/EditorCommandPalette.cpp
    src/EditorCommandPaletteRuntime.cpp
    src/EditorCommandPaletteProbe.cpp
    src/EditorMenuToolbarRuntime.cpp
    src/EditorMenuToolbarRuntimeProbe.cpp
    src/EditorSceneView.cpp
    src/EditorSceneViewProbe.cpp
    src/EditorTransformGizmo.cpp
    src/EditorTransformGizmoProbe.cpp
)

add_library(AK::EditorUI ALIAS ak_editorui)

target_include_directories(ak_editorui
    PUBLIC
        include
)

target_link_libraries(ak_editorui
    PUBLIC
        AK::Core
        AK::ECS
        AK::Commands
)

set_target_properties(ak_editorui PROPERTIES FOLDER "Engine")
