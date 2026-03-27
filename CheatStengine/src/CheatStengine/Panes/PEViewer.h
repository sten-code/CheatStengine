#pragma once

#include <CheatStengine/Panes/Pane.h>

class PEViewer final : public Pane {
public:
    explicit PEViewer(State& state);

    void Draw(double deltaTime) override;

private:
    void DrawModuleList();
    void DrawTabBar();

    void DrawHeadersTab();
    void DrawDataDirectories(const IMAGE_DATA_DIRECTORY* dataDirectories);

    void DrawDataDirectoriesTab();
    void DrawDirectoryDetails(const IMAGE_DATA_DIRECTORY& dir, DWORD index);
    void DrawExportDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir);
    void DrawImportDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir);
    void DrawResourceDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir);
    void DrawDebugDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir);
    void DrawTLSDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir);

private:
    size_t m_SelectedIndex = -1;
    int m_SelectedDirectory = -1;
    std::string m_SearchQuery;
};