#include "RandomIdGenerator.hpp"
#include <openssl/rand.h>
#include <stdexcept>

namespace {
void randomBytes(unsigned char* bytes, int size)
{
    if (RAND_bytes(bytes, size) != 1) {
        throw std::runtime_error("secure random generator unavailable");
    }
}
}

uint64_t IDGenerator::newID()
{
    uint64_t id;
    randomBytes(reinterpret_cast<unsigned char*>(&id), sizeof(id));
    return id;
}

std::string IDGenerator::newSessionToken()
{
    unsigned char bytes[32];
    randomBytes(bytes, sizeof(bytes));
    constexpr char hex[] = "0123456789abcdef";
    std::string token;
    token.reserve(sizeof(bytes) * 2);
    for (unsigned char byte : bytes) {
        token += hex[byte >> 4];
        token += hex[byte & 0x0f];
    }
    return token;
}
