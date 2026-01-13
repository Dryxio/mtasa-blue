/*****************************************************************************
 *
 *  PROJECT:     MTA:SA Server - Android Network Module
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Server/net-android/CNetBitStreamAndroid.h
 *  PURPOSE:     BitStream implementation for Android network module
 *
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <atomic>
#include <pthread.h>

// Forward declare ISyncStructure
struct ISyncStructure;

//=============================================================================
// CCriticalSection - Thread synchronization primitive
// MUST match MTA's CCriticalSection memory layout and behavior!
// deathmatch.so calls Lock() and Unlock() on this object.
//=============================================================================
class CCriticalSection
{
public:
    CCriticalSection()
    {
        m_pCriticalSection = new pthread_mutex_t;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(static_cast<pthread_mutex_t*>(m_pCriticalSection), &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~CCriticalSection()
    {
        if (m_pCriticalSection)
        {
            pthread_mutex_destroy(static_cast<pthread_mutex_t*>(m_pCriticalSection));
            delete static_cast<pthread_mutex_t*>(m_pCriticalSection);
        }
    }

    void Lock()
    {
        if (m_pCriticalSection)
            pthread_mutex_lock(static_cast<pthread_mutex_t*>(m_pCriticalSection));
    }

    void Unlock()
    {
        if (m_pCriticalSection)
            pthread_mutex_unlock(static_cast<pthread_mutex_t*>(m_pCriticalSection));
    }

private:
    void* m_pCriticalSection;  // pthread_mutex_t* on Linux
};

//=============================================================================
// CRefCountable - Reference counting base class
// MUST match MTA's CRefCountable memory layout and behavior!
// Memory layout: [ vtable_ptr | m_iRefCount | m_pCS ]
// deathmatch.so expects m_pCS to point to a valid CCriticalSection!
//=============================================================================
class CRefCountable
{
    int                m_iRefCount;
    CCriticalSection*  m_pCS;
    static CCriticalSection ms_CS;  // Static critical section shared by all instances

protected:
    virtual ~CRefCountable() {}

public:
    CRefCountable() : m_iRefCount(1), m_pCS(&ms_CS) {}

    void AddRef()
    {
        m_pCS->Lock();
        ++m_iRefCount;
        m_pCS->Unlock();
    }

    int Release()
    {
        m_pCS->Lock();
        int newCount = --m_iRefCount;
        m_pCS->Unlock();

        if (newCount == 0)
        {
            delete this;
        }
        return newCount;
    }
};

//=============================================================================
// NetBitStreamInterface - Simplified version for server
//=============================================================================

class NetBitStreamInterface : public CRefCountable
{
public:
    virtual int  GetReadOffsetAsBits() = 0;
    virtual void SetReadOffsetAsBits(int iOffset) = 0;

    virtual void Reset() = 0;
    virtual void ResetReadPointer() = 0;

    virtual void Write(const unsigned char& input) = 0;
    virtual void Write(const char& input) = 0;
    virtual void Write(const unsigned short& input) = 0;
    virtual void Write(const short& input) = 0;
    virtual void Write(const unsigned int& input) = 0;
    virtual void Write(const int& input) = 0;
    virtual void Write(const float& input) = 0;
    virtual void Write(const double& input) = 0;
    virtual void Write(const char* input, int numberOfBytes) = 0;
    virtual void Write(const ISyncStructure* syncStruct) = 0;

    virtual void WriteCompressed(const unsigned char& input) = 0;
    virtual void WriteCompressed(const char& input) = 0;
    virtual void WriteCompressed(const unsigned short& input) = 0;
    virtual void WriteCompressed(const short& input) = 0;
    virtual void WriteCompressed(const unsigned int& input) = 0;
    virtual void WriteCompressed(const int& input) = 0;
    virtual void WriteCompressed(const float& input) = 0;
    virtual void WriteCompressed(const double& input) = 0;

    virtual void WriteBits(const char* input, unsigned int numbits) = 0;
    virtual void WriteBit(bool input) = 0;
    virtual void WriteNormVector(float x, float y, float z) = 0;
    virtual void WriteVector(float x, float y, float z) = 0;
    virtual void WriteNormQuat(float w, float x, float y, float z) = 0;
    virtual void WriteOrthMatrix(float m00, float m01, float m02,
                                  float m10, float m11, float m12,
                                  float m20, float m21, float m22) = 0;

    virtual bool Read(unsigned char& output) = 0;
    virtual bool Read(char& output) = 0;
    virtual bool Read(unsigned short& output) = 0;
    virtual bool Read(short& output) = 0;
    virtual bool Read(unsigned int& output) = 0;
    virtual bool Read(int& output) = 0;
    virtual bool Read(float& output) = 0;
    virtual bool Read(double& output) = 0;
    virtual bool Read(char* output, int numberOfBytes) = 0;
    virtual bool Read(ISyncStructure* syncStruct) = 0;

    virtual bool ReadCompressed(unsigned char& output) = 0;
    virtual bool ReadCompressed(char& output) = 0;
    virtual bool ReadCompressed(unsigned short& output) = 0;
    virtual bool ReadCompressed(short& output) = 0;
    virtual bool ReadCompressed(unsigned int& output) = 0;
    virtual bool ReadCompressed(int& output) = 0;
    virtual bool ReadCompressed(float& output) = 0;
    virtual bool ReadCompressed(double& output) = 0;

    virtual bool ReadBits(char* output, unsigned int numbits) = 0;
    virtual bool ReadBit() = 0;
    virtual bool ReadNormVector(float& x, float& y, float& z) = 0;
    virtual bool ReadVector(float& x, float& y, float& z) = 0;
    virtual bool ReadNormQuat(float& w, float& x, float& y, float& z) = 0;
    virtual bool ReadOrthMatrix(float& m00, float& m01, float& m02,
                                 float& m10, float& m11, float& m12,
                                 float& m20, float& m21, float& m22) = 0;

    virtual int GetNumberOfBitsUsed() const = 0;
    virtual int GetNumberOfBytesUsed() const = 0;
    virtual int GetNumberOfUnreadBits() const = 0;

    virtual void AlignWriteToByteBoundary() const = 0;
    virtual void AlignReadToByteBoundary() const = 0;

    virtual unsigned char* GetData() const = 0;
    virtual unsigned short Version() const = 0;

    //=========================================================================
    // String reading methods - Required by deathmatch.so CPlayerJoinDataPacket
    // These are inline template methods matching MTA's Shared/sdk/net/bitstream.h
    //=========================================================================

    // Check if we can read N bytes
    bool CanReadNumberOfBytes(unsigned int uiLength) const
    {
        return (GetNumberOfUnreadBits() >= (int)(uiLength * 8));
    }

    // Read characters into a std::string (no length prefix)
    bool ReadStringCharacters(std::string& result, unsigned int uiLength)
    {
        printf("[bitstream] ReadStringCharacters(length=%u)\n", uiLength);
        fflush(stdout);
        result = "";
        if (uiLength)
        {
            if (!CanReadNumberOfBytes(uiLength))
            {
                printf("[bitstream]   -> FAILED: not enough bytes (have %d bits, need %u)\n",
                       GetNumberOfUnreadBits(), uiLength * 8);
                fflush(stdout);
                return false;
            }
            std::vector<char> bufferArray;
            bufferArray.resize(uiLength);
            char* buffer = &bufferArray[0];
            if (!Read(buffer, uiLength))
            {
                printf("[bitstream]   -> FAILED: Read() returned false\n");
                fflush(stdout);
                return false;
            }
            result = std::string(buffer, uiLength);
            printf("[bitstream]   -> SUCCESS: read '%s'\n", result.c_str());
            fflush(stdout);
        }
        return true;
    }

    // Read a string with ushort length prefix (default)
    template <typename SizeType = unsigned short>
    bool ReadString(std::string& result)
    {
        printf("[bitstream] ReadString<SizeType>() at bit %d\n", GetReadOffsetAsBits());
        fflush(stdout);
        result = "";
        SizeType length = 0;
        if (!Read(length))
        {
            printf("[bitstream]   -> FAILED: couldn't read length\n");
            fflush(stdout);
            return false;
        }
        printf("[bitstream]   -> length=%u, calling ReadStringCharacters\n", (unsigned)length);
        fflush(stdout);
        return ReadStringCharacters(result, length);
    }

    // Write a string with ushort length prefix
    template <typename SizeType = unsigned short>
    void WriteString(const std::string& value)
    {
        SizeType length = static_cast<SizeType>(value.length());
        Write(length);
        if (length > 0)
            Write(value.c_str(), length);
    }

    // Write string characters (no length prefix)
    void WriteStringCharacters(const std::string& value, unsigned int uiLength)
    {
        if (uiLength > 0 && uiLength <= value.length())
            Write(value.c_str(), uiLength);
    }
};

//=============================================================================
// CNetBitStreamAndroid - BitStream Implementation
//=============================================================================

class CNetBitStreamAndroid : public NetBitStreamInterface
{
public:
    CNetBitStreamAndroid(const void* pData = nullptr, unsigned int uiDataSize = 0,
                          unsigned short usVersion = 0x06B);
    virtual ~CNetBitStreamAndroid();

    // Read/write offset
    virtual int  GetReadOffsetAsBits() override;
    virtual void SetReadOffsetAsBits(int iOffset) override;

    virtual void Reset() override;
    virtual void ResetReadPointer() override;

    // Basic write operations
    virtual void Write(const unsigned char& input) override;
    virtual void Write(const char& input) override;
    virtual void Write(const unsigned short& input) override;
    virtual void Write(const short& input) override;
    virtual void Write(const unsigned int& input) override;
    virtual void Write(const int& input) override;
    virtual void Write(const float& input) override;
    virtual void Write(const double& input) override;
    virtual void Write(const char* input, int numberOfBytes) override;
    virtual void Write(const ISyncStructure* syncStruct) override;

    // Compressed write
    virtual void WriteCompressed(const unsigned char& input) override;
    virtual void WriteCompressed(const char& input) override;
    virtual void WriteCompressed(const unsigned short& input) override;
    virtual void WriteCompressed(const short& input) override;
    virtual void WriteCompressed(const unsigned int& input) override;
    virtual void WriteCompressed(const int& input) override;
    virtual void WriteCompressed(const float& input) override;
    virtual void WriteCompressed(const double& input) override;

    // Bit operations
    virtual void WriteBits(const char* input, unsigned int numbits) override;
    virtual void WriteBit(bool input) override;

    // Vector/quaternion write
    virtual void WriteNormVector(float x, float y, float z) override;
    virtual void WriteVector(float x, float y, float z) override;
    virtual void WriteNormQuat(float w, float x, float y, float z) override;
    virtual void WriteOrthMatrix(float m00, float m01, float m02,
                                  float m10, float m11, float m12,
                                  float m20, float m21, float m22) override;

    // Basic read operations
    virtual bool Read(unsigned char& output) override;
    virtual bool Read(char& output) override;
    virtual bool Read(unsigned short& output) override;
    virtual bool Read(short& output) override;
    virtual bool Read(unsigned int& output) override;
    virtual bool Read(int& output) override;
    virtual bool Read(float& output) override;
    virtual bool Read(double& output) override;
    virtual bool Read(char* output, int numberOfBytes) override;
    virtual bool Read(ISyncStructure* syncStruct) override;

    // Compressed read
    virtual bool ReadCompressed(unsigned char& output) override;
    virtual bool ReadCompressed(char& output) override;
    virtual bool ReadCompressed(unsigned short& output) override;
    virtual bool ReadCompressed(short& output) override;
    virtual bool ReadCompressed(unsigned int& output) override;
    virtual bool ReadCompressed(int& output) override;
    virtual bool ReadCompressed(float& output) override;
    virtual bool ReadCompressed(double& output) override;

    // Bit read operations
    virtual bool ReadBits(char* output, unsigned int numbits) override;
    virtual bool ReadBit() override;

    // Vector/quaternion read
    virtual bool ReadNormVector(float& x, float& y, float& z) override;
    virtual bool ReadVector(float& x, float& y, float& z) override;
    virtual bool ReadNormQuat(float& w, float& x, float& y, float& z) override;
    virtual bool ReadOrthMatrix(float& m00, float& m01, float& m02,
                                 float& m10, float& m11, float& m12,
                                 float& m20, float& m21, float& m22) override;

    // Size information
    virtual int GetNumberOfBitsUsed() const override;
    virtual int GetNumberOfBytesUsed() const override;
    virtual int GetNumberOfUnreadBits() const override;

    // Alignment
    virtual void AlignWriteToByteBoundary() const override;
    virtual void AlignReadToByteBoundary() const override;

    // Data access
    virtual unsigned char* GetData() const override;
    virtual unsigned short Version() const override;

private:
    void EnsureCapacity(unsigned int bitsNeeded);

    std::vector<unsigned char> m_data;
    mutable unsigned int       m_writeBitOffset;
    mutable unsigned int       m_readBitOffset;
    unsigned short             m_version;
};
