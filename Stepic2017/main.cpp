#include <iostream>
#include "Windows.h"
#include "fstream"
#include <vector>
#include <string>

#define SCOPE_EXIT_CAT2(x, y) x##y
#define SCOPE_EXIT_CAT(x, y) SCOPE_EXIT_CAT2(x, y)
#define SCOPE_EXIT auto SCOPE_EXIT_CAT(scopeExit_, __COUNTER__) = uf::MakeScopeExit() += [&]

namespace uf
{
    template<typename F>
    class ScopeExit
    {
    public:
        ScopeExit(F&& f) : m_f(f)
        {
        }

        ~ScopeExit()
        {
            m_f();
        }

        ScopeExit(ScopeExit&& other);

    private:
        ScopeExit(const ScopeExit&);
        ScopeExit& operator=(const ScopeExit&);

    private:
        F m_f;
    };

    struct MakeScopeExit
    {
        template<typename F>
        ScopeExit<F> operator+=(F&& f)
        {
            return ScopeExit<F>(std::move(f));
        }
    };
}

std::vector<std::vector<BYTE>> readFromFileAsync(std::string const& path, std::vector<size_t> offsets)
{
    auto file = CreateFile(path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr);

    SCOPE_EXIT{ CloseHandle(file); };
    if (file == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("failed to open file");
    }

    std::vector<OVERLAPPED> overlappedVector;
    std::vector<std::vector<BYTE>> result(offsets.size());
    for (size_t i = 0; i < offsets.size(); ++i)
    {
        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = CreateEvent(nullptr,
            TRUE,
            FALSE,
            nullptr);

        if (!overlapped.hEvent)
        {
            throw std::exception("failed to create event");
        }

        result[i].resize(offsets[i]);
        if (!ReadFile(file, result[i].data(), static_cast<DWORD>(offsets[i]), nullptr, &overlapped))
        {
            if (GetLastError() != ERROR_IO_PENDING)
            {
                std::cout << GetLastError() << std::endl;
                throw std::exception("failed to read file");
            }
        }

        overlappedVector.push_back(overlapped);
    }

    std::vector<HANDLE> events;
    events.reserve(overlappedVector.size());
    for (auto& element : overlappedVector)
    {
        events.push_back(element.hEvent);
    }

    const auto waitRes = WaitForMultipleObjects(events.size(),
        events.data(),
        TRUE,
        INFINITE);
    if (waitRes == WAIT_FAILED)
    {
        throw std::runtime_error("Error waiting for I/O to finish");
    }

    std::cout << "Strings that were read from file: " << std::endl;

    for (size_t i = 0; i < offsets.size(); ++i)
    {
        std::cout << std::string(result[i].begin(), result[i].end()) << std::endl;
    }

    return result;
}

int main()
{
    // in func hire we should close Event in OVERLAPPED structure
    // (possibly use std::unique_ptr with custom deleter) but as this is example code I omitted this check 
    const std::string testString_1 = "test string 1";
    const std::string testString_2 = "test string 2";
    const std::string path = "test.txt";

    std::ofstream file(path);
    file << testString_1;
    file << testString_2;
    file.close();

    std::vector<size_t> offsets;
    offsets.push_back(testString_1.size());
    offsets.push_back(testString_2.size());

    readFromFileAsync(path, offsets);
}