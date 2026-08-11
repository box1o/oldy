#include <woki/core.hpp>

#include "core/entry/entry.hpp"

int main(int argc, char* argv[]) {
    slog::Configure();
    return studio::Run(argc, argv);
}
