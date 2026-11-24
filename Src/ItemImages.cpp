#include "ItemImages.h"

#include <Windows.h>
#include <winhttp.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "Logging.h"

#pragma comment(lib, "winhttp.lib")

namespace cm {
    namespace {
        constexpr wchar_t c_Host[] = L"media.hitmaps.com";
        constexpr size_t c_MaxImageBytes = 4 * 1024 * 1024;

        bool ReadFile(const std::filesystem::path& p_Path, std::vector<uint8_t>& p_Bytes) {
            std::ifstream s_File(p_Path, std::ios::binary);

            if (!s_File) {
                return false;
            }

            p_Bytes.assign(std::istreambuf_iterator<char>(s_File), std::istreambuf_iterator<char>());
            return !p_Bytes.empty();
        }

        void WriteFile(const std::filesystem::path& p_Path, const std::vector<uint8_t>& p_Bytes) {
            std::ofstream s_File(p_Path, std::ios::binary | std::ios::trunc);

            if (s_File) {
                s_File.write(reinterpret_cast<const char*>(p_Bytes.data()), static_cast<std::streamsize>(p_Bytes.size()));
            }
        }
    }

    ItemImages::~ItemImages() {
        Shutdown();
    }

    bool ItemImages::IsValidId(const std::string& p_Id) {
        // Nur GUIDs – die ID wird zu einem Dateinamen und einer Web-Adresse.
        if (p_Id.size() != 36) {
            return false;
        }

        for (const char c : p_Id) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-')) {
                return false;
            }
        }

        return true;
    }

    void ItemImages::Start(const std::wstring& p_CacheDir) {
        if (!m_Workers.empty()) {
            return;
        }

        m_CacheDir = p_CacheDir;
        std::error_code s_Error;
        std::filesystem::create_directories(m_CacheDir, s_Error);

        m_Stop = false;

        for (int i = 0; i < 3; ++i) {
            m_Workers.emplace_back(&ItemImages::Worker, this);
        }
    }

    void ItemImages::Shutdown() {
        {
            std::scoped_lock s_Lock(m_Mutex);
            m_Stop = true;
            m_Queue.clear();
        }

        m_Cv.notify_all();

        for (auto& s_Worker : m_Workers) {
            if (s_Worker.joinable()) {
                s_Worker.join();
            }
        }

        m_Workers.clear();

        std::scoped_lock s_Lock(m_Mutex);

        for (auto& [s_Id, s_Entry] : m_Entries) {
            if (s_Entry->m_ImGuiTexture.id) {
                SDK()->DestroyImGuiTexture(s_Entry->m_Texture, s_Entry->m_ImGuiTexture);
                s_Entry->m_ImGuiTexture = {};
            }
        }

        m_Entries.clear();
        m_ReadyIds.clear();
    }

    const ImGuiTexture* ItemImages::Get(const std::string& p_RepositoryId, EState* p_State) {
        std::string s_Id = p_RepositoryId;

        for (char& c : s_Id) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (!IsValidId(s_Id) || m_Workers.empty()) {
            if (p_State) *p_State = EState::Missing;
            return nullptr;
        }

        std::scoped_lock s_Lock(m_Mutex);
        auto s_It = m_Entries.find(s_Id);

        if (s_It == m_Entries.end()) {
            m_Entries.emplace(s_Id, std::make_unique<SEntry>());
            m_Queue.push_back(s_Id);
            m_Cv.notify_one();

            if (p_State) *p_State = EState::Loading;
            return nullptr;
        }

        if (p_State) *p_State = s_It->second->m_State;
        return s_It->second->m_State == EState::Loaded ? &s_It->second->m_ImGuiTexture : nullptr;
    }

    void ItemImages::UploadPending(int p_MaxPerFrame) {
        std::vector<std::pair<std::string, std::vector<uint8_t>>> s_Work;

        {
            std::scoped_lock s_Lock(m_Mutex);

            while (!m_ReadyIds.empty() && static_cast<int>(s_Work.size()) < p_MaxPerFrame) {
                const std::string s_Id = m_ReadyIds.back();
                m_ReadyIds.pop_back();

                if (const auto s_It = m_Entries.find(s_Id); s_It != m_Entries.end() && s_It->second->m_State == EState::Ready) {
                    s_Work.emplace_back(s_Id, std::move(s_It->second->m_Bytes));
                }
            }
        }

        for (auto& [s_Id, s_Bytes] : s_Work) {
            ScopedD3DRef<ID3D12Resource> s_Texture;
            ImGuiTexture s_ImGuiTexture {};
            const bool s_Ok = SDK()->CreateWICTextureFromMemory(s_Bytes.data(), s_Bytes.size(), s_Texture, s_ImGuiTexture);

            std::scoped_lock s_Lock(m_Mutex);
            const auto s_It = m_Entries.find(s_Id);

            if (s_It == m_Entries.end()) {
                if (s_Ok) SDK()->DestroyImGuiTexture(s_Texture, s_ImGuiTexture);
                continue;
            }

            if (s_Ok && s_ImGuiTexture.id) {
                s_It->second->m_Texture = std::move(s_Texture);
                s_It->second->m_ImGuiTexture = s_ImGuiTexture;
                s_It->second->m_State = EState::Loaded;
                ++m_Loaded;
            }
            else {
                s_It->second->m_State = EState::Missing;
                ++m_Missing;
            }
        }
    }

    void ItemImages::Worker() {
        while (true) {
            std::string s_Id;

            {
                std::unique_lock s_Lock(m_Mutex);
                m_Cv.wait(s_Lock, [this]() { return m_Stop || !m_Queue.empty(); });

                if (m_Stop) {
                    return;
                }

                // Neueste zuerst: das sind die Karten, die gerade zu sehen sind.
                s_Id = m_Queue.back();
                m_Queue.pop_back();
            }

            const std::filesystem::path s_Image = std::filesystem::path(m_CacheDir) / (s_Id + ".jpg");
            const std::filesystem::path s_NoImage = std::filesystem::path(m_CacheDir) / (s_Id + ".none");

            std::vector<uint8_t> s_Bytes;
            bool s_Found = ReadFile(s_Image, s_Bytes);

            if (!s_Found && !std::filesystem::exists(s_NoImage) && m_Online) {
                bool s_NotFound = false;

                if (Download(s_Id, s_Bytes, s_NotFound)) {
                    WriteFile(s_Image, s_Bytes);
                    s_Found = true;
                }
                else if (s_NotFound) {
                    WriteFile(s_NoImage, {}); // nicht jedes Mal neu fragen
                }
            }

            std::scoped_lock s_Lock(m_Mutex);
            const auto s_It = m_Entries.find(s_Id);

            if (s_It == m_Entries.end()) {
                continue;
            }

            if (s_Found) {
                s_It->second->m_Bytes = std::move(s_Bytes);
                s_It->second->m_State = EState::Ready;
                m_ReadyIds.push_back(s_Id);
            }
            else {
                s_It->second->m_State = EState::Missing;
                ++m_Missing;
            }
        }
    }

    bool ItemImages::Download(const std::string& p_Id, std::vector<uint8_t>& p_Bytes, bool& p_NotFound) const {
        p_NotFound = false;

        const std::wstring s_Path =
            L"/img/hitman3/unlockables/item_perspective_" + std::wstring(p_Id.begin(), p_Id.end()) + L"_0.jpg";

        HINTERNET s_Session = WinHttpOpen(L"CheatMenu/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

        if (!s_Session) {
            return false;
        }

        WinHttpSetTimeouts(s_Session, 5000, 5000, 8000, 8000);
        HINTERNET s_Connection = WinHttpConnect(s_Session, c_Host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        HINTERNET s_Request = s_Connection
            ? WinHttpOpenRequest(s_Connection, L"GET", s_Path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
            : nullptr;

        bool s_Ok = false;

        if (s_Request && WinHttpSendRequest(s_Request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(s_Request, nullptr)) {
            DWORD s_Status = 0;
            DWORD s_Size = sizeof(s_Status);
            WinHttpQueryHeaders(s_Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &s_Status, &s_Size, WINHTTP_NO_HEADER_INDEX);

            if (s_Status == 404 || s_Status == 403) {
                p_NotFound = true;
            }
            else if (s_Status == 200) {
                DWORD s_Available = 0;

                while (!m_Stop && WinHttpQueryDataAvailable(s_Request, &s_Available) && s_Available > 0) {
                    const size_t s_Offset = p_Bytes.size();

                    if (s_Offset + s_Available > c_MaxImageBytes) {
                        p_Bytes.clear();
                        break;
                    }

                    p_Bytes.resize(s_Offset + s_Available);
                    DWORD s_Read = 0;

                    if (!WinHttpReadData(s_Request, p_Bytes.data() + s_Offset, s_Available, &s_Read)) {
                        p_Bytes.clear();
                        break;
                    }

                    p_Bytes.resize(s_Offset + s_Read);
                }

                // Nur echte JPEG-Dateien annehmen (keine Fehlerseiten).
                s_Ok = p_Bytes.size() > 4 && p_Bytes[0] == 0xFF && p_Bytes[1] == 0xD8;
            }
        }

        if (s_Request) WinHttpCloseHandle(s_Request);
        if (s_Connection) WinHttpCloseHandle(s_Connection);
        WinHttpCloseHandle(s_Session);

        if (!s_Ok) {
            p_Bytes.clear();
        }

        return s_Ok;
    }
}
