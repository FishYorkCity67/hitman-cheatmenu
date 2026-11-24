#pragma once

// Echte Bilder der Gegenstaende (Renderbilder aus dem Spiel) fuer das Inventar.
// Quelle: das Bildarchiv von HITMAPS (media.hitmaps.com), Dateiname = Repository-ID des Gegenstands.
// Einmal geladen, liegen sie unter mods\CheatMenu\images\ und werden danach von dort genommen.

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "IModSDK.h"
#include "IRenderer.h"

namespace cm {
    class ItemImages {
    public:
        enum class EState {
            Loading, // wird gesucht/heruntergeladen
            Ready,   // Datei da, Textur noch nicht erzeugt
            Loaded,  // Textur fertig
            Missing, // gibt es nicht (oder kein Internet)
        };

        ItemImages() = default;
        ~ItemImages();

        ItemImages(const ItemImages&) = delete;
        ItemImages& operator=(const ItemImages&) = delete;

        void Start(const std::wstring& p_CacheDir);
        void Shutdown();
        void SetOnline(bool p_Online) { m_Online = p_Online; }

        // Render-Thread: die Textur, falls schon da – sonst nullptr (und das Laden wird angestossen).
        const ImGuiTexture* Get(const std::string& p_RepositoryId, EState* p_State = nullptr);

        // Render-Thread: fertig geladene Dateien in Texturen umwandeln (begrenzt pro Bild, damit nichts ruckelt).
        void UploadPending(int p_MaxPerFrame);

        int LoadedCount() const { return m_Loaded; }
        int MissingCount() const { return m_Missing; }

    private:
        struct SEntry {
            EState m_State = EState::Loading;
            std::vector<uint8_t> m_Bytes;
            ScopedD3DRef<ID3D12Resource> m_Texture;
            ImGuiTexture m_ImGuiTexture {};
        };

        void Worker();
        bool Download(const std::string& p_Id, std::vector<uint8_t>& p_Bytes, bool& p_NotFound) const;
        static bool IsValidId(const std::string& p_Id);

        std::wstring m_CacheDir;
        std::mutex m_Mutex;
        std::condition_variable m_Cv;
        std::unordered_map<std::string, std::unique_ptr<SEntry>> m_Entries;
        std::deque<std::string> m_Queue;
        std::vector<std::string> m_ReadyIds;
        std::vector<std::thread> m_Workers;
        std::atomic<bool> m_Stop = false;
        std::atomic<bool> m_Online = true;
        std::atomic<int> m_Loaded = 0;
        std::atomic<int> m_Missing = 0;
    };
}
