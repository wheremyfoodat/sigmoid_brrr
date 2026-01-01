#include <SDL.h>
#include <fmt/format.h>

#include <cli_args/cli_args.hpp>
#include <fstream>

#include "imgui_impl_sdl2.h"
#include "ui.hpp"

void parseCmdlineArgs(Sigmoid* top, int argc, char** argv);
void printHelp();

void stepCycles(Sigmoid* top, uint cycles) {
    while (cycles > 0) {
        top->clk = 0;
        top->eval();

        top->clk = 1;
        top->eval();

        cycles--;
    }
}

int main(int argc, char** argv) {
    auto ctx = new VerilatedContext();
    auto top = new Sigmoid(ctx, "TOP");

    // Reset crisp
    top->rst = 1;
    top->valid_in = 0;
    stepCycles(top, 10);

    top->rst = 0;
    stepCycles(top, 10);

    parseCmdlineArgs(top, argc, argv);

    auto [window, glContext] = UI::init();

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        UI::startFrame();
        UI::drawTopModule(top);
        UI::drawPipeline(top);
        UI::endFrame(window);
    }

    UI::deinit(window, glContext);
    delete top;
    delete ctx;
}

void parseCmdlineArgs(Sigmoid* top, int argc, char** argv) {
    // Parse commandline flags to see if we should run headless tests
    CommandLine::args args(argc, argv);

    const bool help = args.get<bool>("h").value_or(false) || args.get<bool>("help").value_or(false);
    const bool headless = args.get<bool>("headless").value_or(false);

    if (help) {
        printHelp();
        std::exit(0);
    }

    if (headless) {
        const std::string testCaseFilename = args.get<std::string>("input").value_or("");
        int testsRan = 0, testsFailed = 0;

        if (testCaseFilename.empty()) {
            printf("Headless mode specified but no test case file was provided\n");
            abort();
        }

        std::ifstream inputFile(testCaseFilename);
        if (!inputFile.good() || !inputFile.is_open()) {
            printf("Failed to open input file\n");
            abort();
        }

        while (!inputFile.eof()) {
            uint16_t input, expected;
            inputFile >> std::hex >> input >> std::hex >> expected;

            top->data_in = input;
            top->valid_in = 1;
            top->rst = 0;
            testsRan++;

            // Step for 5 cycles until the result is at the end of the pipeline, then compare with expected output
            stepCycles(top, 5);

            if (top->data_out != expected) {
                testsFailed++;
                fmt::print("Test case failed\n");
                fmt::print("Input: {:04X}\n", input);
                fmt::print("Output: {:04X}, expected: {:04X}\n", top->data_out, expected);
            }
        }

        fmt::print("Tests ran:    {}\n", testsRan);
        fmt::print("Tests passed: {}\n", testsRan - testsFailed);
        fmt::print("Test failed:  {}\n", testsFailed);

        // Exit with an error if we had failures
        std::exit(testsFailed != 0 ? -1 : 0);
    }
}

void printHelp() {
    fmt::print(
        "Options:\n"
        "  -h, --help             Show this help message\n"
        "  --headless             Run tests in headless mode\n"
        "  --input <filename>     Input file for headless testing\n\n"
        "The input file for headless testing should contain test cases in the form:\n"
        "  <input_data> <expected_output>\n"
        "Where both values are bfloat16 hex values\n"
    );
}

// Legacy function required so linking works
double sc_time_stamp() {
    return 0;
}
