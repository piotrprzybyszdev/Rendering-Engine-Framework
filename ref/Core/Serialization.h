#include <bit>
#include <istream>
#include <span>
#include <vector>

namespace ref::serialization
{

template<typename T> inline T Align(T value, T alignment)
{
    assert(std::has_single_bit(alignment));
    return (value + alignment - 1) & ~(alignment - 1);
}

template<typename I, typename T> inline void AddOffset(T &position, T alignment)
{
    position = Align(position + sizeof(I), alignment);
}

template<typename I, typename T> inline void AddOffset(T &position, T alignment, std::span<const I> items)
{
    position = Align(position + items.size_bytes(), alignment);
}

static inline constexpr uint64_t g_MaxAlignment = 16;
static inline constexpr std::array<char, g_MaxAlignment> g_Padding = {};

template<typename I, typename T, typename C>
inline void Serialize(std::basic_ostream<C> &stream, T alignment, const I &item)
{
    assert(alignment <= g_MaxAlignment);
    stream.write(reinterpret_cast<const C *>(&item), sizeof(I));
    uint64_t position = stream.tellp();
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.write(g_Padding.data(), paddingSize);
}

template<typename I, typename T, typename C>
inline void Serialize(std::basic_ostream<C> &stream, T alignment, std::span<const I> items)
{
    assert(alignment <= g_MaxAlignment);
    stream.write(reinterpret_cast<const C *>(items.data()), items.size_bytes());
    uint64_t position = stream.tellp();
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.write(g_Padding.data(), paddingSize);
}

template<typename I, typename T, typename C>
inline void Deserialize(std::basic_istream<C> &stream, T alignment, I &item)
{
    assert(alignment <= g_MaxAlignment);
    [[maybe_unused]] uint64_t before = stream.tellg();
    stream.read(reinterpret_cast<C *>(&item), sizeof(I));
    uint64_t position = stream.tellg();
    assert(position - before == sizeof(I));
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.ignore(paddingSize);
    [[maybe_unused]] uint64_t after = stream.tellg();
    assert(after - position == paddingSize);
}

template<typename I, typename T, typename C>
inline void Deserialize(std::basic_istream<C> &stream, T alignment, size_t count, std::vector<I> &items)
{
    assert(alignment <= g_MaxAlignment);
    items.resize(count);
    [[maybe_unused]] uint64_t before = stream.tellg();
    stream.read(reinterpret_cast<C *>(items.data()), count * sizeof(I));
    uint64_t position = stream.tellg();
    assert(position - before == count * sizeof(I));
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.ignore(paddingSize);
    [[maybe_unused]] uint64_t after = stream.tellg();
    assert(after - position == paddingSize);
}

inline constexpr uint32_t MakeMagic(const char magic[4])
{
    return magic[0] << 24u | magic[1] << 16u | magic[2] << 8u | magic[3];
}

inline constexpr uint32_t MakeVersion(uint32_t variant, uint32_t major, uint32_t minor, uint32_t patch)
{
    return variant << 29u | major << 22u | minor << 12u | patch;
}

}