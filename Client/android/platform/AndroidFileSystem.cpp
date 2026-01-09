/*
 * MTA:SA Android - File System Implementation
 */

#include "AndroidFileSystem.h"
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <sys/statvfs.h>
#define LOG_TAG "MTA-FileSystem"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) printf(__VA_ARGS__)
#define LOGD(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace MTA::Android::Platform
{

//=============================================================================
// FileHandle Implementation
//=============================================================================

FileHandle::FileHandle()
    : m_handle(nullptr)
    , m_asset(nullptr)
    , m_size(0)
{
}

FileHandle::~FileHandle()
{
    Close();
}

size_t FileHandle::Read(void* buffer, size_t size)
{
    if (m_asset)
    {
#ifdef __ANDROID__
        return AAsset_read(m_asset, buffer, size);
#else
        return 0;
#endif
    }
    else if (m_handle)
    {
        return fread(buffer, 1, size, m_handle);
    }
    return 0;
}

size_t FileHandle::Write(const void* buffer, size_t size)
{
    if (m_asset)
    {
        // Assets are read-only
        return 0;
    }
    else if (m_handle)
    {
        return fwrite(buffer, 1, size, m_handle);
    }
    return 0;
}

bool FileHandle::Seek(int64_t offset, int origin)
{
    if (m_asset)
    {
#ifdef __ANDROID__
        return AAsset_seek64(m_asset, offset, origin) >= 0;
#else
        return false;
#endif
    }
    else if (m_handle)
    {
        return fseek(m_handle, offset, origin) == 0;
    }
    return false;
}

int64_t FileHandle::Tell() const
{
    if (m_asset)
    {
#ifdef __ANDROID__
        return AAsset_getLength64(m_asset) - AAsset_getRemainingLength64(m_asset);
#else
        return 0;
#endif
    }
    else if (m_handle)
    {
        return ftell(m_handle);
    }
    return -1;
}

int64_t FileHandle::GetSize() const
{
    return m_size;
}

void FileHandle::Close()
{
    if (m_asset)
    {
#ifdef __ANDROID__
        AAsset_close(m_asset);
#endif
        m_asset = nullptr;
    }

    if (m_handle)
    {
        fclose(m_handle);
        m_handle = nullptr;
    }

    m_size = 0;
}

//=============================================================================
// DirectoryIterator Implementation
//=============================================================================

DirectoryIterator::DirectoryIterator(const std::string& path)
    : m_path(path)
    , m_dir(nullptr)
    , m_isAsset(false)
    , m_assetDir(nullptr)
{
    // Check if this is an asset path
    if (path.find("assets://") == 0 || path.find("@assets/") == 0)
    {
        m_isAsset = true;
#ifdef __ANDROID__
        std::string assetPath = path;
        if (assetPath.find("assets://") == 0)
            assetPath = assetPath.substr(9);
        else if (assetPath.find("@assets/") == 0)
            assetPath = assetPath.substr(8);

        m_assetDir = AAssetManager_openDir(
            AndroidFileSystem::Instance().GetAssetManager(),
            assetPath.c_str());
#endif
    }
    else
    {
        m_dir = opendir(path.c_str());
    }
}

DirectoryIterator::~DirectoryIterator()
{
    if (m_assetDir)
    {
#ifdef __ANDROID__
        AAssetDir_close(static_cast<AAssetDir*>(m_assetDir));
#endif
    }

    if (m_dir)
    {
        closedir(static_cast<DIR*>(m_dir));
    }
}

bool DirectoryIterator::Next(FileInfo& outInfo)
{
    if (m_isAsset)
    {
#ifdef __ANDROID__
        if (!m_assetDir)
            return false;

        const char* name = AAssetDir_getNextFileName(static_cast<AAssetDir*>(m_assetDir));
        if (!name)
            return false;

        outInfo.name = name;
        outInfo.path = AndroidFileSystem::JoinPath(m_path, name);
        outInfo.isDirectory = false;  // Assets don't have directory info
        outInfo.isReadOnly = true;
        outInfo.size = 0;
        outInfo.modifiedTime = 0;
        return true;
#else
        return false;
#endif
    }
    else
    {
        if (!m_dir)
            return false;

        struct dirent* entry;
        while ((entry = readdir(static_cast<DIR*>(m_dir))) != nullptr)
        {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            outInfo.name = entry->d_name;
            outInfo.path = AndroidFileSystem::JoinPath(m_path, entry->d_name);
            outInfo.isDirectory = (entry->d_type == DT_DIR);

            // Get additional info
            struct stat st;
            if (stat(outInfo.path.c_str(), &st) == 0)
            {
                outInfo.size = st.st_size;
                outInfo.modifiedTime = st.st_mtime;
                outInfo.isReadOnly = (access(outInfo.path.c_str(), W_OK) != 0);
            }

            return true;
        }
        return false;
    }
}

void DirectoryIterator::Reset()
{
    if (m_isAsset)
    {
#ifdef __ANDROID__
        if (m_assetDir)
        {
            AAssetDir_rewind(static_cast<AAssetDir*>(m_assetDir));
        }
#endif
    }
    else
    {
        if (m_dir)
        {
            rewinddir(static_cast<DIR*>(m_dir));
        }
    }
}

//=============================================================================
// AndroidFileSystem Implementation
//=============================================================================

AndroidFileSystem::AndroidFileSystem()
    : m_initialized(false)
    , m_assetManager(nullptr)
{
}

AndroidFileSystem::~AndroidFileSystem()
{
    Shutdown();
}

#ifdef __ANDROID__
bool AndroidFileSystem::Initialize(JNIEnv* env, jobject context, AAssetManager* assetManager)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
        return true;

    LOGI("Initializing Android file system...");

    m_assetManager = assetManager;

    // Get paths from Android context
    jclass contextClass = env->GetObjectClass(context);

    // Internal files directory
    jmethodID getFilesDir = env->GetMethodID(contextClass, "getFilesDir", "()Ljava/io/File;");
    jobject filesDir = env->CallObjectMethod(context, getFilesDir);
    if (filesDir)
    {
        jclass fileClass = env->GetObjectClass(filesDir);
        jmethodID getAbsolutePath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
        jstring pathStr = (jstring)env->CallObjectMethod(filesDir, getAbsolutePath);
        const char* path = env->GetStringUTFChars(pathStr, nullptr);
        m_internalPath = path;
        env->ReleaseStringUTFChars(pathStr, path);
        env->DeleteLocalRef(pathStr);
        env->DeleteLocalRef(filesDir);
    }

    // External files directory
    jmethodID getExternalFilesDir = env->GetMethodID(contextClass, "getExternalFilesDir",
        "(Ljava/lang/String;)Ljava/io/File;");
    jobject externalDir = env->CallObjectMethod(context, getExternalFilesDir, nullptr);
    if (externalDir)
    {
        jclass fileClass = env->GetObjectClass(externalDir);
        jmethodID getAbsolutePath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
        jstring pathStr = (jstring)env->CallObjectMethod(externalDir, getAbsolutePath);
        const char* path = env->GetStringUTFChars(pathStr, nullptr);
        m_externalPath = path;
        env->ReleaseStringUTFChars(pathStr, path);
        env->DeleteLocalRef(pathStr);
        env->DeleteLocalRef(externalDir);
    }

    // Cache directory
    jmethodID getCacheDir = env->GetMethodID(contextClass, "getCacheDir", "()Ljava/io/File;");
    jobject cacheDir = env->CallObjectMethod(context, getCacheDir);
    if (cacheDir)
    {
        jclass fileClass = env->GetObjectClass(cacheDir);
        jmethodID getAbsolutePath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
        jstring pathStr = (jstring)env->CallObjectMethod(cacheDir, getAbsolutePath);
        const char* path = env->GetStringUTFChars(pathStr, nullptr);
        m_cachePath = path;
        env->ReleaseStringUTFChars(pathStr, path);
        env->DeleteLocalRef(pathStr);
        env->DeleteLocalRef(cacheDir);
    }

    // OBB directory
    jmethodID getObbDir = env->GetMethodID(contextClass, "getObbDir", "()Ljava/io/File;");
    jobject obbDir = env->CallObjectMethod(context, getObbDir);
    if (obbDir)
    {
        jclass fileClass = env->GetObjectClass(obbDir);
        jmethodID getAbsolutePath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
        jstring pathStr = (jstring)env->CallObjectMethod(obbDir, getAbsolutePath);
        const char* path = env->GetStringUTFChars(pathStr, nullptr);
        m_obbPath = path;
        env->ReleaseStringUTFChars(pathStr, path);
        env->DeleteLocalRef(pathStr);
        env->DeleteLocalRef(obbDir);
    }

    // Set MTA data path
    m_mtaDataPath = JoinPath(m_externalPath, "MTA");
    CreateDirectory(m_mtaDataPath);

    LOGI("Paths initialized:");
    LOGI("  Internal: %s", m_internalPath.c_str());
    LOGI("  External: %s", m_externalPath.c_str());
    LOGI("  Cache: %s", m_cachePath.c_str());
    LOGI("  OBB: %s", m_obbPath.c_str());
    LOGI("  MTA: %s", m_mtaDataPath.c_str());

    // Try to detect game installation
    DetectGameInstallation();

    m_initialized = true;
    LOGI("Android file system initialized");

    return true;
}
#else
bool AndroidFileSystem::Initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
        return true;

    // Non-Android fallback paths
    m_internalPath = "./data/internal";
    m_externalPath = "./data/external";
    m_cachePath = "./data/cache";
    m_obbPath = "./data/obb";
    m_mtaDataPath = "./data/mta";
    m_gameDataPath = "./data/gtasa";

    m_initialized = true;
    return true;
}
#endif

void AndroidFileSystem::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
        return;

    LOGI("Shutting down Android file system");

    m_assetManager = nullptr;
    m_initialized = false;
}

std::string AndroidFileSystem::GetBasePath(StorageType type) const
{
    switch (type)
    {
        case StorageType::Internal:  return m_internalPath;
        case StorageType::External:  return m_externalPath;
        case StorageType::Assets:    return "assets://";
        case StorageType::OBB:       return m_obbPath;
        case StorageType::Cache:     return m_cachePath;
        case StorageType::GameData:  return m_gameDataPath;
        default:                     return m_externalPath;
    }
}

std::string AndroidFileSystem::ResolvePath(const std::string& virtualPath) const
{
    // Handle virtual path prefixes
    if (virtualPath.find("@mods/") == 0)
        return JoinPath(m_mtaDataPath, "mods/" + virtualPath.substr(6));
    if (virtualPath.find("@resources/") == 0)
        return JoinPath(m_mtaDataPath, "resources/" + virtualPath.substr(11));
    if (virtualPath.find("@cache/") == 0)
        return JoinPath(m_cachePath, virtualPath.substr(7));
    if (virtualPath.find("@game/") == 0)
        return JoinPath(m_gameDataPath, virtualPath.substr(6));
    if (virtualPath.find("@assets/") == 0)
        return virtualPath;  // Keep as-is for asset manager

    // Absolute path
    if (virtualPath[0] == '/')
        return virtualPath;

    // Relative to MTA data
    return JoinPath(m_mtaDataPath, virtualPath);
}

void AndroidFileSystem::SetGameDataPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gameDataPath = path;
    LOGI("Game data path set to: %s", path.c_str());
}

bool AndroidFileSystem::FileExists(const std::string& path) const
{
    std::string resolvedPath = ResolvePath(path);

    if (IsAssetPath(resolvedPath))
    {
#ifdef __ANDROID__
        if (!m_assetManager)
            return false;

        std::string assetPath = StripAssetPrefix(resolvedPath);
        AAsset* asset = AAssetManager_open(m_assetManager, assetPath.c_str(), AASSET_MODE_UNKNOWN);
        if (asset)
        {
            AAsset_close(asset);
            return true;
        }
        return false;
#else
        return false;
#endif
    }

    struct stat st;
    return stat(resolvedPath.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool AndroidFileSystem::DirectoryExists(const std::string& path) const
{
    std::string resolvedPath = ResolvePath(path);

    struct stat st;
    return stat(resolvedPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

int64_t AndroidFileSystem::GetFileSize(const std::string& path) const
{
    std::string resolvedPath = ResolvePath(path);

    if (IsAssetPath(resolvedPath))
    {
#ifdef __ANDROID__
        if (!m_assetManager)
            return -1;

        std::string assetPath = StripAssetPrefix(resolvedPath);
        AAsset* asset = AAssetManager_open(m_assetManager, assetPath.c_str(), AASSET_MODE_UNKNOWN);
        if (asset)
        {
            int64_t size = AAsset_getLength64(asset);
            AAsset_close(asset);
            return size;
        }
        return -1;
#else
        return -1;
#endif
    }

    struct stat st;
    if (stat(resolvedPath.c_str(), &st) == 0)
        return st.st_size;
    return -1;
}

bool AndroidFileSystem::GetFileInfo(const std::string& path, FileInfo& outInfo) const
{
    std::string resolvedPath = ResolvePath(path);

    struct stat st;
    if (stat(resolvedPath.c_str(), &st) != 0)
        return false;

    outInfo.path = resolvedPath;
    outInfo.name = GetFileName(resolvedPath);
    outInfo.size = st.st_size;
    outInfo.modifiedTime = st.st_mtime;
    outInfo.isDirectory = S_ISDIR(st.st_mode);
    outInfo.isReadOnly = (access(resolvedPath.c_str(), W_OK) != 0);

    return true;
}

FileHandle* AndroidFileSystem::OpenFile(const std::string& path, const char* mode)
{
    std::string resolvedPath = ResolvePath(path);

    auto handle = new FileHandle();

    if (IsAssetPath(resolvedPath))
    {
#ifdef __ANDROID__
        if (!m_assetManager)
        {
            delete handle;
            return nullptr;
        }

        std::string assetPath = StripAssetPrefix(resolvedPath);
        handle->m_asset = AAssetManager_open(m_assetManager, assetPath.c_str(), AASSET_MODE_STREAMING);
        if (!handle->m_asset)
        {
            delete handle;
            return nullptr;
        }
        handle->m_size = AAsset_getLength64(handle->m_asset);
#else
        delete handle;
        return nullptr;
#endif
    }
    else
    {
        handle->m_handle = fopen(resolvedPath.c_str(), mode);
        if (!handle->m_handle)
        {
            delete handle;
            return nullptr;
        }

        // Get size
        fseek(handle->m_handle, 0, SEEK_END);
        handle->m_size = ftell(handle->m_handle);
        fseek(handle->m_handle, 0, SEEK_SET);
    }

    return handle;
}

void AndroidFileSystem::CloseFile(FileHandle* handle)
{
    if (handle)
    {
        handle->Close();
        delete handle;
    }
}

std::vector<uint8_t> AndroidFileSystem::ReadFile(const std::string& path)
{
    std::vector<uint8_t> result;

    FileHandle* handle = OpenFile(path, "rb");
    if (!handle)
        return result;

    int64_t size = handle->GetSize();
    if (size > 0)
    {
        result.resize(size);
        handle->Read(result.data(), size);
    }

    CloseFile(handle);
    return result;
}

std::string AndroidFileSystem::ReadFileAsString(const std::string& path)
{
    auto data = ReadFile(path);
    return std::string(data.begin(), data.end());
}

bool AndroidFileSystem::WriteFile(const std::string& path, const void* data, size_t size)
{
    std::string resolvedPath = ResolvePath(path);

    // Create parent directory if needed
    std::string dir = GetDirectory(resolvedPath);
    if (!dir.empty())
        CreateDirectory(dir);

    FILE* fp = fopen(resolvedPath.c_str(), "wb");
    if (!fp)
    {
        LOGE("Failed to open file for writing: %s", resolvedPath.c_str());
        return false;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    return written == size;
}

bool AndroidFileSystem::WriteFile(const std::string& path, const std::string& content)
{
    return WriteFile(path, content.data(), content.size());
}

bool AndroidFileSystem::DeleteFile(const std::string& path)
{
    std::string resolvedPath = ResolvePath(path);
    return unlink(resolvedPath.c_str()) == 0;
}

bool AndroidFileSystem::CopyFile(const std::string& source, const std::string& dest)
{
    auto data = ReadFile(source);
    if (data.empty() && GetFileSize(source) > 0)
        return false;

    return WriteFile(dest, data.data(), data.size());
}

bool AndroidFileSystem::MoveFile(const std::string& source, const std::string& dest)
{
    std::string srcPath = ResolvePath(source);
    std::string dstPath = ResolvePath(dest);

    // Try rename first (efficient for same filesystem)
    if (rename(srcPath.c_str(), dstPath.c_str()) == 0)
        return true;

    // Fall back to copy + delete
    if (CopyFile(source, dest))
    {
        DeleteFile(source);
        return true;
    }

    return false;
}

bool AndroidFileSystem::CreateDirectory(const std::string& path)
{
    std::string resolvedPath = ResolvePath(path);
    return CreateDirectoryInternal(resolvedPath);
}

bool AndroidFileSystem::CreateDirectoryInternal(const std::string& path)
{
    std::string current;
    for (size_t i = 0; i < path.length(); i++)
    {
        current += path[i];
        if (path[i] == '/' || i == path.length() - 1)
        {
            if (!current.empty() && current != "/")
            {
                struct stat st;
                if (stat(current.c_str(), &st) != 0)
                {
                    if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST)
                    {
                        LOGE("Failed to create directory: %s", current.c_str());
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool AndroidFileSystem::RemoveDirectory(const std::string& path, bool recursive)
{
    std::string resolvedPath = ResolvePath(path);

    if (recursive)
    {
        auto files = ListDirectory(resolvedPath);
        for (const auto& file : files)
        {
            if (file.isDirectory)
                RemoveDirectory(file.path, true);
            else
                DeleteFile(file.path);
        }
    }

    return rmdir(resolvedPath.c_str()) == 0;
}

std::vector<FileInfo> AndroidFileSystem::ListDirectory(const std::string& path)
{
    std::vector<FileInfo> result;

    DirectoryIterator iter(ResolvePath(path));
    FileInfo info;
    while (iter.Next(info))
    {
        result.push_back(info);
    }

    return result;
}

std::vector<std::string> AndroidFileSystem::FindFiles(
    const std::string& path,
    const std::string& pattern,
    bool recursive)
{
    std::vector<std::string> result;
    auto files = ListDirectory(path);

    for (const auto& file : files)
    {
        if (file.isDirectory)
        {
            if (recursive)
            {
                auto subFiles = FindFiles(file.path, pattern, true);
                result.insert(result.end(), subFiles.begin(), subFiles.end());
            }
        }
        else
        {
            // Simple pattern matching (*.ext)
            if (pattern == "*" || pattern == "*.*")
            {
                result.push_back(file.path);
            }
            else if (pattern.length() > 1 && pattern[0] == '*')
            {
                std::string ext = pattern.substr(1);
                if (file.name.length() >= ext.length() &&
                    file.name.substr(file.name.length() - ext.length()) == ext)
                {
                    result.push_back(file.path);
                }
            }
        }
    }

    return result;
}

bool AndroidFileSystem::DetectGameInstallation()
{
    LOGI("Detecting GTA:SA installation...");

    // Common locations for GTA:SA Android
    std::vector<std::string> searchPaths = {
        "/sdcard/Android/data/com.rockstargames.gtasa/files",
        "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files",
        JoinPath(m_obbPath, "../com.rockstargames.gtasa/files"),
        "/sdcard/Android/obb/com.rockstargames.gtasa",
    };

    for (const auto& searchPath : searchPaths)
    {
        // Check for gta3.img (main game archive)
        std::string gta3img = JoinPath(searchPath, "models/gta3.img");
        if (FileExists(gta3img))
        {
            m_gameDataPath = searchPath;
            LOGI("Found GTA:SA at: %s", searchPath.c_str());
            return true;
        }

        // Also check for textures folder
        std::string textures = JoinPath(searchPath, "textures");
        if (DirectoryExists(textures))
        {
            m_gameDataPath = searchPath;
            LOGI("Found GTA:SA at: %s", searchPath.c_str());
            return true;
        }
    }

    LOGI("GTA:SA installation not found automatically");
    return false;
}

bool AndroidFileSystem::GameFileExists(const std::string& relativePath) const
{
    return FileExists(JoinPath(m_gameDataPath, relativePath));
}

FileHandle* AndroidFileSystem::OpenGameFile(const std::string& relativePath)
{
    return OpenFile(JoinPath(m_gameDataPath, relativePath), "rb");
}

std::string AndroidFileSystem::GetGameFilePath(const std::string& relativePath) const
{
    return JoinPath(m_gameDataPath, relativePath);
}

uint64_t AndroidFileSystem::GetFreeSpace(StorageType type) const
{
#ifdef __ANDROID__
    std::string path = GetBasePath(type);
    struct statvfs st;
    if (statvfs(path.c_str(), &st) == 0)
    {
        return (uint64_t)st.f_bavail * st.f_frsize;
    }
#endif
    return 0;
}

uint64_t AndroidFileSystem::GetTotalSpace(StorageType type) const
{
#ifdef __ANDROID__
    std::string path = GetBasePath(type);
    struct statvfs st;
    if (statvfs(path.c_str(), &st) == 0)
    {
        return (uint64_t)st.f_blocks * st.f_frsize;
    }
#endif
    return 0;
}

std::string AndroidFileSystem::NormalizePath(const std::string& path)
{
    std::string result = path;

    // Replace backslashes with forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove double slashes
    size_t pos;
    while ((pos = result.find("//")) != std::string::npos)
    {
        result.erase(pos, 1);
    }

    return result;
}

bool AndroidFileSystem::IsAssetPath(const std::string& path) const
{
    return path.find("assets://") == 0 || path.find("@assets/") == 0;
}

std::string AndroidFileSystem::StripAssetPrefix(const std::string& path) const
{
    if (path.find("assets://") == 0)
        return path.substr(9);
    if (path.find("@assets/") == 0)
        return path.substr(8);
    return path;
}

bool AndroidFileSystem::AssetExists(const std::string& assetPath) const
{
    return FileExists("assets://" + assetPath);
}

std::vector<uint8_t> AndroidFileSystem::ReadAsset(const std::string& assetPath)
{
    return ReadFile("assets://" + assetPath);
}

std::vector<std::string> AndroidFileSystem::ListAssets(const std::string& assetPath)
{
    std::vector<std::string> result;

#ifdef __ANDROID__
    if (!m_assetManager)
        return result;

    AAssetDir* dir = AAssetManager_openDir(m_assetManager, assetPath.c_str());
    if (!dir)
        return result;

    const char* filename;
    while ((filename = AAssetDir_getNextFileName(dir)) != nullptr)
    {
        result.push_back(filename);
    }

    AAssetDir_close(dir);
#endif

    return result;
}

} // namespace MTA::Android::Platform
