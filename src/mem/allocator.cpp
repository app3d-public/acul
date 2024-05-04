#include <core/mem/allocator.hpp>
#include <string>

template class MemAllocator<int8_t>;
template class MemAllocator<int16_t>;
template class MemAllocator<int32_t>;
template class MemAllocator<int64_t>;
template class MemAllocator<uint8_t>;
template class MemAllocator<uint16_t>;
template class MemAllocator<uint32_t>;
template class MemAllocator<uint64_t>;
template class MemAllocator<float>;
template class MemAllocator<double>;
template class MemAllocator<char>;
template class MemAllocator<char16_t>;
template class MemAllocator<char32_t>;
template class MemAllocator<std::string>;