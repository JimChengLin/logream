#include <iostream>

namespace logream {
    namespace compress_bench {
        void Run();
    }
    namespace lite_bench {
        void Run();
    }
}

int main() {
    logream::compress_bench::Run();
    logream::lite_bench::Run();
    std::cout << "Done." << std::endl;
    return 0;
}