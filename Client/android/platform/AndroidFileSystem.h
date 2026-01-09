/*
 * MTA:SA Android - File System Abstraction
 *
 * Provides platform-independent file system access for Android:
 *   - Internal storage (app-private)
 *   - External storage (shared, requires permissions)
 *   - Assets (read-only, bundled with APK)
 *   - OBB expansion files
 *   - GTA:SA game data location
 *
 * Design:
 *   - Abstracts Android's complex storage model
 *   - Provides paths compatible with existing MTA code
 *   - Handles Android 10+ scoped storage
 *   - Thread-safe file operations
 */

#ifndef ANDROID_FILESYSTEM_H
#define ANDROID_FILESYSTEM_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

namespace MTA::Android::Platform
{

//=============================================================================
// Path Types
//=============================================================================

enum class StorageType
{
    Internal,       // App-private internal storage
    External,       // Shared external storage (requires permission)
    Assets,         // Read-only APK assets
    OBB,            // Expansion files
    Cache,          // Temporary cache storage
    GameData        // GTA:SA data location
};

//=============================================================================
// File Info Structure
//=============================================================================

struct FileInfo
{
    std::string name;
    std::string path;
    uint64_t size;
    uint64_t modifiedTime;
    bool isDirectory;
    bool isReadOnly;
};

//=============================================================================
// File Handle for streaming
//=============================================================================

class FileHandle
{
public:
    FileHandle();
    ~FileHandle();

    bool IsOpen() const { return m_handle != nullptr || m_asset != nullptr; }
    bool IsAsset() const { return m_asset != nullptr; }

    size_t Read(void* buffer, size_t size);
    size_t Write(const void* buffer, size_t size);
    bool Seek(int64_t offset, int origin);
    int64_t Tell() const;
    int64_t GetSize() const;
    void Close();

private:
    friend class AndroidFileSystem;

    FILE* m_handle;
#ifdef __ANDROID__
    AAsset* m_asset;
#else
    void* m_asset;
#endif
    int64_t m_size;
};

//=============================================================================
// Directory Iterator
//=============================================================================

class DirectoryIterator
{
public:
    DirectoryIterator(const std::string& path);
    ~DirectoryIterator();

    bool Next(FileInfo& outInfo);
    void Reset();

private:
    std::string m_path;
    void* m_dir;
    bool m_isAsset;
#ifdef __ANDROID__
    AAssetDir* m_assetDir;
#else
    void* m_assetDir;
#endif
};

//=============================================================================
// AndroidFileSystem - Main File System Interface
//=============================================================================

class AndroidFileSystem
{
public:
    static AndroidFileSystem& Instance();

    //=========================================================================
    // Initialization
    //=========================================================================

    /**
     * Initialize with Android environment
     * Must be called from JNI with valid context
     */
#ifdef __ANDROID__
    bool Initialize(JNIEnv* env, jobject context, AAssetManager* assetManager);
#else
    bool Initialize();
#endif

    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    //=========================================================================
    // Path Resolution
    //=========================================================================

    /**
     * Get the base path for a storage type
     */
    std::string GetBasePath(StorageType type) const;

    /**
     * Resolve a virtual path to an absolute path
     * Handles MTA path prefixes like "@mods/", "@resources/", etc.
     */
    std::string ResolvePath(const std::string& virtualPath) const;

    /**
     * Get GTA:SA data directory
     */
    std::string GetGameDataPath() const { return m_gameDataPath; }

    /**
     * Set GTA:SA data directory (detected or user-specified)
     */
    void SetGameDataPath(const std::string& path);

    /**
     * Get MTA data directory (for resources, mods, etc.)
     */
    std::string GetMTADataPath() const { return m_mtaDataPath; }

    /**
     * Get Android asset manager (for reading APK assets)
     */
#ifdef __ANDROID__
    AAssetManager* GetAssetManager() const { return m_assetManager; }
#else
    void* GetAssetManager() const { return m_assetManager; }
#endif

    //=========================================================================
    // File Operations
    //=========================================================================

    /**
     * Check if a file exists
     */
    bool FileExists(const std::string& path) const;

    /**
     * Check if a directory exists
     */
    bool DirectoryExists(const std::string& path) const;

    /**
     * Get file size
     */
    int64_t GetFileSize(const std::string& path) const;

    /**
     * Get file info
     */
    bool GetFileInfo(const std::string& path, FileInfo& outInfo) const;

    /**
     * Open a file
     * @param path File path (can be virtual or absolute)
     * @param mode "r", "w", "a", "rb", "wb", etc.
     */
    FileHandle* OpenFile(const std::string& path, const char* mode);

    /**
     * Close a file handle
     */
    void CloseFile(FileHandle* handle);

    /**
     * Read entire file into buffer
     */
    std::vector<uint8_t> ReadFile(const std::string& path);

    /**
     * Read entire file as string
     */
    std::string ReadFileAsString(const std::string& path);

    /**
     * Write buffer to file
     */
    bool WriteFile(const std::string& path, const void* data, size_t size);

    /**
     * Write string to file
     */
    bool WriteFile(const std::string& path, const std::string& content);

    /**
     * Delete a file
     */
    bool DeleteFile(const std::string& path);

    /**
     * Copy a file
     */
    bool CopyFile(const std::string& source, const std::string& dest);

    /**
     * Move/rename a file
     */
    bool MoveFile(const std::string& source, const std::string& dest);

    //=========================================================================
    // Directory Operations
    //=========================================================================

    /**
     * Create a directory (and parents if needed)
     */
    bool CreateDirectory(const std::string& path);

    /**
     * Remove a directory
     * @param recursive If true, remove contents as well
     */
    bool RemoveDirectory(const std::string& path, bool recursive = false);

    /**
     * List directory contents
     */
    std::vector<FileInfo> ListDirectory(const std::string& path);

    /**
     * Find files matching a pattern
     * @param path Base directory
     * @param pattern Glob pattern (*.txt, *.lua, etc.)
     * @param recursive Search subdirectories
     */
    std::vector<std::string> FindFiles(
        const std::string& path,
        const std::string& pattern,
        bool recursive = false);

    //=========================================================================
    // Asset Operations (Read-only APK resources)
    //=========================================================================

    /**
     * Check if an asset exists
     */
    bool AssetExists(const std::string& assetPath) const;

    /**
     * Read an asset file
     */
    std::vector<uint8_t> ReadAsset(const std::string& assetPath);

    /**
     * List assets in a directory
     */
    std::vector<std::string> ListAssets(const std::string& assetPath);

    //=========================================================================
    // GTA:SA Specific
    //=========================================================================

    /**
     * Detect GTA:SA installation
     * Searches common locations and OBB files
     */
    bool DetectGameInstallation();

    /**
     * Check if a game file exists
     */
    bool GameFileExists(const std::string& relativePath) const;

    /**
     * Open a game file
     */
    FileHandle* OpenGameFile(const std::string& relativePath);

    /**
     * Get the path to a game resource
     */
    std::string GetGameFilePath(const std::string& relativePath) const;

    //=========================================================================
    // Utilities
    //=========================================================================

    /**
     * Get free space on storage
     */
    uint64_t GetFreeSpace(StorageType type) const;

    /**
     * Get total space on storage
     */
    uint64_t GetTotalSpace(StorageType type) const;

    /**
     * Normalize path separators
     */
    static std::string NormalizePath(const std::string& path);

    /**
     * Get file extension
     */
    static std::string GetExtension(const std::string& path);

    /**
     * Get filename from path
     */
    static std::string GetFileName(const std::string& path);

    /**
     * Get directory from path
     */
    static std::string GetDirectory(const std::string& path);

    /**
     * Join path components
     */
    static std::string JoinPath(const std::string& a, const std::string& b);

private:
    AndroidFileSystem();
    ~AndroidFileSystem();
    AndroidFileSystem(const AndroidFileSystem&) = delete;
    AndroidFileSystem& operator=(const AndroidFileSystem&) = delete;

    // Internal helpers
    bool IsAssetPath(const std::string& path) const;
    std::string StripAssetPrefix(const std::string& path) const;
    bool CreateDirectoryInternal(const std::string& path);

private:
    bool m_initialized;

    // Storage paths
    std::string m_internalPath;
    std::string m_externalPath;
    std::string m_cachePath;
    std::string m_obbPath;
    std::string m_gameDataPath;
    std::string m_mtaDataPath;

    // Android asset manager
#ifdef __ANDROID__
    AAssetManager* m_assetManager;
#else
    void* m_assetManager;
#endif

    // Thread safety
    mutable std::mutex m_mutex;
};

//=============================================================================
// Inline Implementations
//=============================================================================

inline AndroidFileSystem& AndroidFileSystem::Instance()
{
    static AndroidFileSystem instance;
    return instance;
}

inline std::string AndroidFileSystem::JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;

    char lastA = a[a.length() - 1];
    char firstB = b[0];

    if (lastA == '/' || lastA == '\\')
    {
        if (firstB == '/' || firstB == '\\')
            return a + b.substr(1);
        return a + b;
    }
    else
    {
        if (firstB == '/' || firstB == '\\')
            return a + b;
        return a + "/" + b;
    }
}

inline std::string AndroidFileSystem::GetExtension(const std::string& path)
{
    size_t dotPos = path.rfind('.');
    size_t slashPos = path.rfind('/');

    if (dotPos == std::string::npos)
        return "";
    if (slashPos != std::string::npos && dotPos < slashPos)
        return "";

    return path.substr(dotPos);
}

inline std::string AndroidFileSystem::GetFileName(const std::string& path)
{
    size_t slashPos = path.rfind('/');
    if (slashPos == std::string::npos)
        slashPos = path.rfind('\\');

    if (slashPos == std::string::npos)
        return path;

    return path.substr(slashPos + 1);
}

inline std::string AndroidFileSystem::GetDirectory(const std::string& path)
{
    size_t slashPos = path.rfind('/');
    if (slashPos == std::string::npos)
        slashPos = path.rfind('\\');

    if (slashPos == std::string::npos)
        return "";

    return path.substr(0, slashPos);
}

} // namespace MTA::Android::Platform

#endif // ANDROID_FILESYSTEM_H
