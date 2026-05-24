#include <AK/EditorUI/EditorDragDropProbe.hpp>
#include <AK/EditorUI/EditorDragDrop.hpp>

#include <sstream>

namespace AK
{
    EditorDragDropProbeResult RunEditorDragDropProbe()
    {
        EditorDragDropProbeResult result{};
        const EditorDragDropDiagnostics diagnostics = RunEditorDragDropDiagnostics();
        result.ok = diagnostics.ok;

        std::ostringstream out;
        out << "editor drag/drop foundation"
            << " targets=" << diagnostics.targetCount
            << " accepting=" << diagnostics.acceptingTargetCount
            << " invalidRects=" << diagnostics.invalidTargetRectCount
            << " phase=" << ToString(diagnostics.phase)
            << " op=" << ToString(diagnostics.operation);
        result.summary = out.str();
        if (!diagnostics.ok)
        {
            result.summary += "\n" + diagnostics.summary;
        }
        return result;
    }

    std::string BuildEditorDragDropProbeSummary()
    {
        return RunEditorDragDropProbe().summary;
    }
}
