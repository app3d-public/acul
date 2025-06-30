#pragma once

#include "../queue.hpp"
#include "../vector.hpp"

namespace acul
{
    namespace gpu
    {
        template <typename T, typename Alloc>
        class resource_pool
        {
        public:
            Alloc allocator;

            void allocate(size_t size)
            {
                _size = size;
                _pos = 0;
                _data.resize(_size);
                allocator.alloc(_data.data(), _size);
            }

            void destroy()
            {
                for (auto &data : _data) allocator.release(data);
                _data.clear();
                _size = 0;
                _pos = 0;
            }

            void request(T *pData, size_t size)
            {
                int i = 0;
                // Use available data
                for (; size > 0 && _pos < _size; ++i, ++_pos, --size) pData[i] = _data[_pos];
                if (size == 0) return;

                // Use released buffers
                while (size > 0 && !_released.empty())
                {
                    size_t id = _released.front();
                    _released.pop();
                    pData[i++] = _data[id];
                    --size;
                }
                if (size > 0)
                {
                    size_t old_size = _data.size();
                    _data.resize(old_size + size);
                    allocator.alloc(_data.data() + old_size, size);
                    std::copy_n(_data.data() + old_size, size, pData + i);
                    _size = _data.size();
                    _pos = _size;
                }
            }

            void release(T data)
            {
                auto it = std::find(_data.begin(), _data.end(), data);
                if (it != _data.end())
                {
                    size_t index = std::distance(_data.begin(), it);
                    _released.push(index);
                }
            }

            void release(T *pData, size_t size)
            {
                for (size_t i = 0; i < size; ++i) release(pData[i]);
            }

            size_t size() const { return _size - _pos + _released.size(); }

        private:
            size_t _size = 0;
            size_t _pos = 0;
            vector<T> _data;
            queue<size_t> _released;
        };
    } // namespace gpu
} // namespace acul