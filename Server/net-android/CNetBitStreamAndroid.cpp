/*****************************************************************************
 *
 *  PROJECT:     MTA:SA Server - Android Network Module
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Server/net-android/CNetBitStreamAndroid.cpp
 *  PURPOSE:     BitStream implementation for Android network module
 *
 *****************************************************************************/

#include "CNetBitStreamAndroid.h"
#include <cstring>
#include <algorithm>
#include <cmath>

//=============================================================================
// Constructor / Destructor
//=============================================================================

CNetBitStreamAndroid::CNetBitStreamAndroid(const void* pData, unsigned int uiDataSize,
                                           unsigned short usVersion)
    : m_writeBitOffset(0)
    , m_readBitOffset(0)
    , m_version(usVersion)
{
    if (pData && uiDataSize > 0)
    {
        m_data.resize(uiDataSize);
        memcpy(m_data.data(), pData, uiDataSize);
        m_writeBitOffset = uiDataSize * 8;
    }
    else
    {
        m_data.reserve(256);  // Initial capacity
    }
}

CNetBitStreamAndroid::~CNetBitStreamAndroid()
{
}

//=============================================================================
// Offset Management
//=============================================================================

int CNetBitStreamAndroid::GetReadOffsetAsBits()
{
    return m_readBitOffset;
}

void CNetBitStreamAndroid::SetReadOffsetAsBits(int iOffset)
{
    m_readBitOffset = std::max(0, std::min(iOffset, (int)m_writeBitOffset));
}

void CNetBitStreamAndroid::Reset()
{
    m_data.clear();
    m_writeBitOffset = 0;
    m_readBitOffset = 0;
}

void CNetBitStreamAndroid::ResetReadPointer()
{
    m_readBitOffset = 0;
}

//=============================================================================
// Capacity Management
//=============================================================================

void CNetBitStreamAndroid::EnsureCapacity(unsigned int bitsNeeded)
{
    unsigned int bytesNeeded = (m_writeBitOffset + bitsNeeded + 7) / 8;
    if (bytesNeeded > m_data.size())
    {
        m_data.resize(bytesNeeded + 32);  // Add some extra
    }
}

//=============================================================================
// Basic Write Operations
//=============================================================================

void CNetBitStreamAndroid::Write(const unsigned char& input)
{
    WriteBits((const char*)&input, 8);
}

void CNetBitStreamAndroid::Write(const char& input)
{
    WriteBits(&input, 8);
}

void CNetBitStreamAndroid::Write(const unsigned short& input)
{
    WriteBits((const char*)&input, 16);
}

void CNetBitStreamAndroid::Write(const short& input)
{
    WriteBits((const char*)&input, 16);
}

void CNetBitStreamAndroid::Write(const unsigned int& input)
{
    WriteBits((const char*)&input, 32);
}

void CNetBitStreamAndroid::Write(const int& input)
{
    WriteBits((const char*)&input, 32);
}

void CNetBitStreamAndroid::Write(const float& input)
{
    WriteBits((const char*)&input, 32);
}

void CNetBitStreamAndroid::Write(const double& input)
{
    WriteBits((const char*)&input, 64);
}

void CNetBitStreamAndroid::Write(const char* input, int numberOfBytes)
{
    if (numberOfBytes > 0)
    {
        WriteBits(input, numberOfBytes * 8);
    }
}

void CNetBitStreamAndroid::Write(const ISyncStructure* syncStruct)
{
    // SyncStructure writes itself
    if (syncStruct)
    {
        // Cast away const and call Write
        // ISyncStructure should handle this properly
    }
}

//=============================================================================
// Compressed Write Operations
//=============================================================================

void CNetBitStreamAndroid::WriteCompressed(const unsigned char& input)
{
    if (input == 0)
    {
        WriteBit(false);
    }
    else
    {
        WriteBit(true);
        Write(input);
    }
}

void CNetBitStreamAndroid::WriteCompressed(const char& input)
{
    WriteCompressed((unsigned char)input);
}

void CNetBitStreamAndroid::WriteCompressed(const unsigned short& input)
{
    if ((input & 0xFF00) == 0)
    {
        WriteBit(true);
        Write((unsigned char)(input & 0xFF));
    }
    else
    {
        WriteBit(false);
        Write(input);
    }
}

void CNetBitStreamAndroid::WriteCompressed(const short& input)
{
    WriteCompressed((unsigned short)input);
}

void CNetBitStreamAndroid::WriteCompressed(const unsigned int& input)
{
    if ((input & 0xFFFF0000) == 0)
    {
        WriteBit(true);
        WriteCompressed((unsigned short)(input & 0xFFFF));
    }
    else
    {
        WriteBit(false);
        Write(input);
    }
}

void CNetBitStreamAndroid::WriteCompressed(const int& input)
{
    WriteCompressed((unsigned int)input);
}

void CNetBitStreamAndroid::WriteCompressed(const float& input)
{
    Write(input);  // No compression for floats
}

void CNetBitStreamAndroid::WriteCompressed(const double& input)
{
    Write(input);  // No compression for doubles
}

//=============================================================================
// Bit Operations
//=============================================================================

void CNetBitStreamAndroid::WriteBits(const char* input, unsigned int numbits)
{
    if (numbits == 0)
        return;

    EnsureCapacity(numbits);

    // Fast path: byte-aligned write
    if ((m_writeBitOffset % 8) == 0)
    {
        unsigned int startByte = m_writeBitOffset / 8;
        unsigned int fullBytes = numbits / 8;
        unsigned int remainingBits = numbits % 8;

        if (fullBytes > 0)
        {
            memcpy(m_data.data() + startByte, input, fullBytes);
            m_writeBitOffset += fullBytes * 8;
        }

        if (remainingBits > 0)
        {
            unsigned char mask = (1 << remainingBits) - 1;
            m_data[m_writeBitOffset / 8] = input[fullBytes] & mask;
            m_writeBitOffset += remainingBits;
        }
    }
    else
    {
        // Slow path: bit-by-bit write
        for (unsigned int i = 0; i < numbits; i++)
        {
            bool bit = (input[i / 8] >> (i % 8)) & 1;
            WriteBit(bit);
        }
    }
}

void CNetBitStreamAndroid::WriteBit(bool input)
{
    EnsureCapacity(1);

    unsigned int byteOffset = m_writeBitOffset / 8;
    unsigned int bitOffset = m_writeBitOffset % 8;

    if (bitOffset == 0)
    {
        m_data[byteOffset] = 0;
    }

    if (input)
    {
        m_data[byteOffset] |= (1 << bitOffset);
    }

    m_writeBitOffset++;
}

void CNetBitStreamAndroid::WriteNormVector(float x, float y, float z)
{
    // Simplified - write as compressed floats
    Write(x);
    Write(y);
    Write(z);
}

void CNetBitStreamAndroid::WriteVector(float x, float y, float z)
{
    Write(x);
    Write(y);
    Write(z);
}

void CNetBitStreamAndroid::WriteNormQuat(float w, float x, float y, float z)
{
    Write(w);
    Write(x);
    Write(y);
    Write(z);
}

void CNetBitStreamAndroid::WriteOrthMatrix(float m00, float m01, float m02,
                                            float m10, float m11, float m12,
                                            float m20, float m21, float m22)
{
    Write(m00); Write(m01); Write(m02);
    Write(m10); Write(m11); Write(m12);
    Write(m20); Write(m21); Write(m22);
}

//=============================================================================
// Basic Read Operations
//=============================================================================

bool CNetBitStreamAndroid::Read(unsigned char& output)
{
    return ReadBits((char*)&output, 8);
}

bool CNetBitStreamAndroid::Read(char& output)
{
    return ReadBits(&output, 8);
}

bool CNetBitStreamAndroid::Read(unsigned short& output)
{
    return ReadBits((char*)&output, 16);
}

bool CNetBitStreamAndroid::Read(short& output)
{
    return ReadBits((char*)&output, 16);
}

bool CNetBitStreamAndroid::Read(unsigned int& output)
{
    return ReadBits((char*)&output, 32);
}

bool CNetBitStreamAndroid::Read(int& output)
{
    return ReadBits((char*)&output, 32);
}

bool CNetBitStreamAndroid::Read(float& output)
{
    return ReadBits((char*)&output, 32);
}

bool CNetBitStreamAndroid::Read(double& output)
{
    return ReadBits((char*)&output, 64);
}

bool CNetBitStreamAndroid::Read(char* output, int numberOfBytes)
{
    if (numberOfBytes <= 0)
        return true;
    return ReadBits(output, numberOfBytes * 8);
}

bool CNetBitStreamAndroid::Read(ISyncStructure* syncStruct)
{
    // SyncStructure reads itself
    return syncStruct != nullptr;
}

//=============================================================================
// Compressed Read Operations
//=============================================================================

bool CNetBitStreamAndroid::ReadCompressed(unsigned char& output)
{
    if (ReadBit())
    {
        return Read(output);
    }
    else
    {
        output = 0;
        return true;
    }
}

bool CNetBitStreamAndroid::ReadCompressed(char& output)
{
    return ReadCompressed((unsigned char&)output);
}

bool CNetBitStreamAndroid::ReadCompressed(unsigned short& output)
{
    if (ReadBit())
    {
        unsigned char low;
        if (!Read(low))
            return false;
        output = low;
        return true;
    }
    else
    {
        return Read(output);
    }
}

bool CNetBitStreamAndroid::ReadCompressed(short& output)
{
    return ReadCompressed((unsigned short&)output);
}

bool CNetBitStreamAndroid::ReadCompressed(unsigned int& output)
{
    if (ReadBit())
    {
        unsigned short low;
        if (!ReadCompressed(low))
            return false;
        output = low;
        return true;
    }
    else
    {
        return Read(output);
    }
}

bool CNetBitStreamAndroid::ReadCompressed(int& output)
{
    return ReadCompressed((unsigned int&)output);
}

bool CNetBitStreamAndroid::ReadCompressed(float& output)
{
    return Read(output);
}

bool CNetBitStreamAndroid::ReadCompressed(double& output)
{
    return Read(output);
}

//=============================================================================
// Bit Read Operations
//=============================================================================

bool CNetBitStreamAndroid::ReadBits(char* output, unsigned int numbits)
{
    if (numbits == 0)
        return true;

    if (m_readBitOffset + numbits > m_writeBitOffset)
        return false;

    // Fast path: byte-aligned read
    if ((m_readBitOffset % 8) == 0)
    {
        unsigned int startByte = m_readBitOffset / 8;
        unsigned int fullBytes = numbits / 8;
        unsigned int remainingBits = numbits % 8;

        if (fullBytes > 0)
        {
            memcpy(output, m_data.data() + startByte, fullBytes);
            m_readBitOffset += fullBytes * 8;
        }

        if (remainingBits > 0)
        {
            unsigned char mask = (1 << remainingBits) - 1;
            output[fullBytes] = m_data[m_readBitOffset / 8] & mask;
            m_readBitOffset += remainingBits;
        }
    }
    else
    {
        // Slow path: bit-by-bit read
        memset(output, 0, (numbits + 7) / 8);
        for (unsigned int i = 0; i < numbits; i++)
        {
            if (ReadBit())
            {
                output[i / 8] |= (1 << (i % 8));
            }
        }
    }

    return true;
}

bool CNetBitStreamAndroid::ReadBit()
{
    if (m_readBitOffset >= m_writeBitOffset)
        return false;

    unsigned int byteOffset = m_readBitOffset / 8;
    unsigned int bitOffset = m_readBitOffset % 8;

    m_readBitOffset++;

    return (m_data[byteOffset] >> bitOffset) & 1;
}

bool CNetBitStreamAndroid::ReadNormVector(float& x, float& y, float& z)
{
    return Read(x) && Read(y) && Read(z);
}

bool CNetBitStreamAndroid::ReadVector(float& x, float& y, float& z)
{
    return Read(x) && Read(y) && Read(z);
}

bool CNetBitStreamAndroid::ReadNormQuat(float& w, float& x, float& y, float& z)
{
    return Read(w) && Read(x) && Read(y) && Read(z);
}

bool CNetBitStreamAndroid::ReadOrthMatrix(float& m00, float& m01, float& m02,
                                           float& m10, float& m11, float& m12,
                                           float& m20, float& m21, float& m22)
{
    return Read(m00) && Read(m01) && Read(m02) &&
           Read(m10) && Read(m11) && Read(m12) &&
           Read(m20) && Read(m21) && Read(m22);
}

//=============================================================================
// Size Information
//=============================================================================

int CNetBitStreamAndroid::GetNumberOfBitsUsed() const
{
    return m_writeBitOffset;
}

int CNetBitStreamAndroid::GetNumberOfBytesUsed() const
{
    return (m_writeBitOffset + 7) / 8;
}

int CNetBitStreamAndroid::GetNumberOfUnreadBits() const
{
    return m_writeBitOffset - m_readBitOffset;
}

//=============================================================================
// Alignment
//=============================================================================

void CNetBitStreamAndroid::AlignWriteToByteBoundary() const
{
    if (m_writeBitOffset % 8 != 0)
    {
        const_cast<CNetBitStreamAndroid*>(this)->m_writeBitOffset =
            ((m_writeBitOffset + 7) / 8) * 8;
    }
}

void CNetBitStreamAndroid::AlignReadToByteBoundary() const
{
    if (m_readBitOffset % 8 != 0)
    {
        const_cast<CNetBitStreamAndroid*>(this)->m_readBitOffset =
            ((m_readBitOffset + 7) / 8) * 8;
    }
}

//=============================================================================
// Data Access
//=============================================================================

unsigned char* CNetBitStreamAndroid::GetData() const
{
    return const_cast<unsigned char*>(m_data.data());
}

unsigned short CNetBitStreamAndroid::Version() const
{
    return m_version;
}
