#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

class Benchmarker {
private:
    struct Measurement {
        std::string name;
        double time_ms;
    };

    struct BlockStats {
        std::string name;
        std::vector<double> times;
        double min;
        double max;
        double mean;
        int count;
        bool has_stats;  // True if multiple measurements

        BlockStats(const std::string& n) : name(n), count(0), has_stats(false) {}

        void add_measurement(double time) {
            times.push_back(time);
            count++;

            if (count >= 2) {
                has_stats = true;
                min = *std::min_element(times.begin(), times.end());
                max = *std::max_element(times.begin(), times.end());
                mean = std::accumulate(times.begin(), times.end(), 0.0) / count;
            }
        }
    };

    std::map<std::string, BlockStats> blocks;
    std::chrono::high_resolution_clock::time_point current_start;
    std::string current_block_name;
    bool is_measuring;

public:
    Benchmarker() : is_measuring(false) {}

    // Start measuring a block with a name
    void start(const std::string& name) {
        // If already measuring, automatically stop the previous block
        if (is_measuring) {
            stop();
        }

        current_block_name = name;
        current_start = std::chrono::high_resolution_clock::now();
        is_measuring = true;

        // Initialize block if it doesn't exist
        if (blocks.find(name) == blocks.end()) {
            blocks.emplace(name, BlockStats(name));
        }
    }

    // Stop measuring the current block
    void stop() {
        if (!is_measuring) {
            std::cerr << "Warning: stop() called without an active measurement\n";
            return;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - current_start);
        double time_ms = duration.count();

        // Add measurement to the block
        try {
            // .at() throws std::out_of_range if current_block_name is not found
            auto& block = blocks.at(current_block_name);
            block.add_measurement(time_ms);
        }
	catch (const std::out_of_range& e) {
            // Handle the missing key scenario here
            std::cerr << "Error: Block '" << current_block_name << "' not found! " << e.what() << '\n';
        }

        is_measuring = false;
    }

    // Convenience: measure a function with automatic start/stop
    template<typename Func>
    void measure(const std::string& name, Func func) {
        start(name);
        func();
        stop();
    }

    // Print the results table
    void print_results() const {
        if (blocks.empty()) {
            std::cout << "No measurements recorded.\n";
            return;
        }

        // Calculate column widths
        size_t name_width = 25;
        for (const auto& pair : blocks) {
            name_width = std::max(name_width, pair.first.length() + 2);
        }

        const int width = name_width + 55;

        // Print header
        std::cout << "\n" << std::string(width, '=') << "\n";
        std::cout << "BENCHMARK RESULTS\n";
        std::cout << std::string(width, '=') << "\n";

        // Print column headers
        std::cout << std::left << std::setw(name_width) << "Block Name";
        std::cout << std::right;
        std::cout << std::setw(12) << "Count";
        std::cout << std::setw(12) << "Min (ms)";
        std::cout << std::setw(12) << "Max (ms)";
        std::cout << std::setw(12) << "Mean (ms)";
        std::cout << std::setw(12) << "Status";
        std::cout << "\n" << std::string(width, '-') << "\n";

        // Print each block
        for (const auto& pair : blocks) {
            const auto& stats = pair.second;

            std::cout << std::left << std::setw(name_width) << stats.name;
            std::cout << std::right << std::fixed << std::setprecision(3);
            std::cout << std::setw(12) << stats.count;

            if (stats.has_stats) {
                std::cout << std::setw(12) << stats.min;
                std::cout << std::setw(12) << stats.max;
                std::cout << std::setw(12) << stats.mean;
                std::cout << std::setw(12) << "Multi";
            } else if (stats.count == 1) {
                std::cout << std::setw(12) << stats.times[0];
                std::cout << std::setw(12) << "-";
                std::cout << std::setw(12) << "-";
                std::cout << std::setw(12) << "Single";
            } else {
                std::cout << std::setw(12) << "-";
                std::cout << std::setw(12) << "-";
                std::cout << std::setw(12) << "-";
                std::cout << std::setw(12) << "Empty";
            }
            std::cout << "\n";
        }

        std::cout << std::string(width, '=') << "\n";

        // Additional summary
        std::cout << "\nLegend:\n";
        std::cout << "  Multi = Multiple measurements (min/max/mean available)\n";
        std::cout << "  Single   = Single measurement (only the recorded time shown)\n";
        std::cout << "\n";
    }

    // Reset all measurements
    void clear() {
        blocks.clear();
        is_measuring = false;
    }

    // Get stats for a specific block
    const BlockStats* get_stats(const std::string& name) const {
        auto it = blocks.find(name);
        if (it != blocks.end()) {
            return &it->second;
        }
        return nullptr;
    }
};
