// Pure C++ module with no .build file: the driver compiles .cpp as C++ from the
// extension, and the link must automatically go through the C++ driver (clang++/g++)
// so the C++ standard library is pulled in without any extra flags.

#ifndef __cplusplus
    #error This translation unit must be compiled as C++
#endif

#include <iostream>
#include <string>
#include <vector>

int main()
{
    int Result = 1;

    // touch iostream + std::string + std::vector so a link that misses the C++
    // standard library fails loudly
    const std::vector<std::string> Parts = { "OK", "cpp", "files" };

    std::string Line;
    for (const std::string& Part : Parts)
    {
        if (!Line.empty())
        {
            Line += " ";
        }

        Line += Part;
    }

    if (Line == "OK cpp files")
    {
        std::cout << Line << "\n";
        Result = 0;
    }
    else
    {
        std::cout << "FAIL cpp files\n";
    }

    return Result;
}
