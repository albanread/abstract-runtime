#include "../../include/abstract_runtime.h"
#include "../../include/lua_bindings.h"
#include "../../include/input_system.h"
#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <atomic>
#include <unordered_set>
#include <regex>
#include <memory>
#include <map>

using namespace AbstractRuntime;

/**
 * Modern Lua Test Runner for Abstract Runtime
 * 
 * Features:
 * - Automatic test discovery in lua_tests/ folder
 * - Console-based output with detailed logging
 * - Both headless and graphics modes
 * - Comprehensive test result reporting
 * - Modern C++ with proper error handling
 */

class LuaTestRunner {
public:
    enum class TestCategory {
        BASIC,
        PERFORMANCE,
        INTEGRATION,
        GRAPHICS,
        CONSOLE,
        RUNTIME
    };

    struct TestResult {
        std::string test_name;
        std::string file_path;
        TestCategory category = TestCategory::BASIC;
        bool passed = false;
        std::string error_message;
        double execution_time = 0.0;
        std::vector<std::string> output_lines;
        std::string skip_reason;
        bool skipped = false;
        
        // Performance metrics
        double memory_usage_mb = 0.0;
        int lua_allocations = 0;
        double peak_frame_time = 0.0;
    };

private:
    lua_State* lua_state_ = nullptr;
    std::vector<TestResult> test_results_;
    std::vector<std::string> current_test_output_;
    bool verbose_mode_ = false;
    bool headless_mode_ = false;
    std::atomic<bool> tests_completed_{false};
    std::atomic<bool> all_tests_passed_{true};
    
    // Test filtering and categorization
    std::unordered_set<TestCategory> enabled_categories_;
    std::regex test_filter_regex_;
    bool has_test_filter_ = false;
    
    // Performance tracking
    std::chrono::high_resolution_clock::time_point runner_start_time_;
    double total_execution_time_ = 0.0;
    size_t tests_run_ = 0;
    size_t tests_skipped_ = 0;

public:
    LuaTestRunner(bool verbose = false, bool headless = false) 
        : verbose_mode_(verbose), headless_mode_(headless) {
        // Enable all categories by default
        enabled_categories_ = {
            TestCategory::BASIC,
            TestCategory::PERFORMANCE,
            TestCategory::INTEGRATION,
            TestCategory::GRAPHICS,
            TestCategory::CONSOLE,
            TestCategory::RUNTIME
        };
        runner_start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    ~LuaTestRunner() {
        cleanup();
    }
    
    // Modern test filtering methods
    void set_test_filter(const std::string& pattern) {
        if (!pattern.empty()) {
            test_filter_regex_ = std::regex(pattern, std::regex_constants::icase);
            has_test_filter_ = true;
        }
    }
    
    void enable_category(TestCategory category) {
        enabled_categories_.insert(category);
    }
    
    void disable_category(TestCategory category) {
        enabled_categories_.erase(category);
    }
    
    void set_categories(const std::unordered_set<TestCategory>& categories) {
        enabled_categories_ = categories;
    }
    
    // Helper method to categorize tests based on filename
    TestCategory categorize_test(const std::string& filename) {
        if (filename.find("performance") != std::string::npos || 
            filename.find("benchmark") != std::string::npos) {
            return TestCategory::PERFORMANCE;
        }
        if (filename.find("graphics") != std::string::npos || 
            filename.find("visual") != std::string::npos) {
            return TestCategory::GRAPHICS;
        }
        if (filename.find("console") != std::string::npos || 
            filename.find("output") != std::string::npos) {
            return TestCategory::CONSOLE;
        }
        if (filename.find("integration") != std::string::npos || 
            filename.find("system") != std::string::npos) {
            return TestCategory::INTEGRATION;
        }
        if (filename.find("runtime") != std::string::npos) {
            return TestCategory::RUNTIME;
        }
        return TestCategory::BASIC;
    }
    
    // Check if test should be run based on filters
    bool should_run_test(const std::string& test_name, TestCategory category) {
        // Check category filter
        if (enabled_categories_.find(category) == enabled_categories_.end()) {
            return false;
        }
        
        // Check regex filter
        if (has_test_filter_) {
            return std::regex_search(test_name, test_filter_regex_);
        }
        
        return true;
    }

    bool initialize() {
        console_section("Lua Test Runner Initialization");
        
        console_info("Creating LuaJIT state for testing...");
        lua_state_ = luaL_newstate();
        if (!lua_state_) {
            console_error("Failed to create LuaJIT state!");
            return false;
        }
        console_info("LuaJIT state created successfully");
        
        // Open standard libraries
        console_info("Loading standard Lua libraries...");
        luaL_openlibs(lua_state_);
        
        // Initialize Abstract Runtime bindings
        console_info("Initializing Abstract Runtime Lua bindings...");
        luaopen_abstract_runtime(lua_state_);
        
        // Register custom test functions
        register_test_functions();
        
        // Set test environment globals
        lua_pushstring(lua_state_, "Modern Lua Test Runner v2.0");
        lua_setglobal(lua_state_, "_TEST_RUNNER_VERSION");
        
        lua_pushboolean(lua_state_, true);
        lua_setglobal(lua_state_, "_TESTING");
        
        lua_pushboolean(lua_state_, verbose_mode_);
        lua_setglobal(lua_state_, "_VERBOSE");
        
        console_info("Test environment initialized successfully");
        return true;
    }
    
    void cleanup() {
        if (lua_state_) {
            console_info("Cleaning up LuaJIT state...");
            lua_close(lua_state_);
            lua_state_ = nullptr;
        }
    }
    
    std::vector<std::string> discover_test_files() {
        console_section("Test Discovery");
        
        std::vector<std::string> test_files;
        std::vector<std::string> search_paths = {
            "lua_tests/",
            "../lua_tests/",
            "../../lua_tests/",  // From new_demos/tests/ directory
            "./tests/lua/",      // Alternative structure
            "../tests/lua/"      // Alternative structure
        };
        
        for (const auto& path : search_paths) {
            console_printf("Searching for tests in: %s", path.c_str());
            
            try {
                if (std::filesystem::exists(path)) {
                    for (const auto& entry : std::filesystem::directory_iterator(path)) {
                        if (entry.is_regular_file()) {
                            std::string filename = entry.path().filename().string();
                            std::string extension = entry.path().extension().string();
                            
                            // Look for .lua files that start with "test_" or end with "_test"
                            if (extension == ".lua" && 
                                (filename.substr(0, 5) == "test_" || 
                                 filename.find("_test.lua") != std::string::npos)) {
                                test_files.push_back(entry.path().string());
                                console_printf("  Found test: %s", filename.c_str());
                            }
                        }
                    }
                    if (!test_files.empty()) {
                        break; // Found tests, stop searching other paths
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                console_warning(e.what());
            }
        }
        
        // Sort for consistent execution order
        std::sort(test_files.begin(), test_files.end());
        
        console_printf("Discovered %zu test files", test_files.size());
        return test_files;
    }
    
    // Helper to convert category to string
    std::string category_to_string(TestCategory category) {
        switch (category) {
            case TestCategory::BASIC: return "BASIC";
            case TestCategory::PERFORMANCE: return "PERFORMANCE";
            case TestCategory::INTEGRATION: return "INTEGRATION";
            case TestCategory::GRAPHICS: return "GRAPHICS";
            case TestCategory::CONSOLE: return "CONSOLE";
            case TestCategory::RUNTIME: return "RUNTIME";
            default: return "UNKNOWN";
        }
    }
    
    bool run_test_file(const std::string& filepath) {
        std::filesystem::path path(filepath);
        std::string test_name = path.filename().string();
        
        TestResult result;
        result.test_name = test_name;
        result.file_path = filepath;
        result.category = categorize_test(test_name);
        current_test_output_.clear();
        
        // Check if test should be run based on filters
        if (!should_run_test(test_name, result.category)) {
            result.skipped = true;
            result.skip_reason = "Filtered out by category or pattern";
            test_results_.push_back(result);
            tests_skipped_++;
            console_printf("Skipping test: %s [%s] - %s", 
                test_name.c_str(), 
                category_to_string(result.category).c_str(),
                result.skip_reason.c_str());
            return true;
        }
        
        console_printf("Running test: %s [%s]", test_name.c_str(), category_to_string(result.category).c_str());
        
        // Read test file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.error_message = "Could not open test file";
            console_error(result.error_message.c_str());
            test_results_.push_back(result);
            return false;
        }
        
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        file.close();
        
        if (content.empty()) {
            result.error_message = "Test file is empty";
            console_warning(result.error_message.c_str());
            test_results_.push_back(result);
            return false;
        }
        
        // Execute test with enhanced error handling
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Set up error handling for better diagnostics
        lua_pushcfunction(lua_state_, [](lua_State* L) -> int {
            const char* msg = lua_tostring(L, 1);
            luaL_traceback(L, L, msg, 1);
            return 1;
        });
        
        int lua_result = luaL_loadstring(lua_state_, content.c_str());
        if (lua_result == LUA_OK) {
            lua_result = lua_pcall(lua_state_, 0, 0, -2);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end_time - start_time);
        result.execution_time = duration.count();
        total_execution_time_ += result.execution_time;
        
        if (lua_result != LUA_OK) {
            // Test failed - enhanced error reporting
            const char* error = lua_tostring(lua_state_, -1);
            result.error_message = error ? error : "Unknown Lua error";
            result.passed = false;
            all_tests_passed_ = false;
            
            console_printf("  ❌ FAILED: %s", result.error_message.c_str());
            
            // Enhanced error context for performance tests
            if (result.category == TestCategory::PERFORMANCE) {
                console_warning("Performance test failed - this may indicate a regression");
            }
            
            lua_pop(lua_state_, 2); // Remove error message and error handler
        } else {
            // Test passed
            result.passed = true;
            tests_run_++;
            
            // Enhanced success reporting with performance indicators
            std::string status_icon = "✅";
            if (result.category == TestCategory::PERFORMANCE && result.execution_time > 5.0) {
                status_icon = "⚠️ ";
                console_warning("Performance test took longer than expected");
            }
            
            console_printf("  %s PASSED (%.3fs)", status_icon.c_str(), result.execution_time);
            
            if (verbose_mode_ && !current_test_output_.empty()) {
                console_info("Test output:");
                for (const auto& output_line : current_test_output_) {
                    console_printf("    %s", output_line.c_str());
                }
            }
        }
        
        result.output_lines = current_test_output_;
        test_results_.push_back(result);
        
        return result.passed;
    }
    
    void run_all_tests(const std::vector<std::string>& test_files) {
        console_section("Running Lua Tests");
        console_printf("Executing %zu test files", test_files.size());
        
        // Show enabled categories
        console_info("Enabled categories:");
        for (const auto& category : enabled_categories_) {
            console_printf("  - %s", category_to_string(category).c_str());
        }
        
        if (has_test_filter_) {
            console_info("Using test filter pattern");
        }
        
        console_separator();
        
        auto suite_start = std::chrono::high_resolution_clock::now();
        bool all_passed = true;
        
        for (const auto& test_file : test_files) {
            bool passed = run_test_file(test_file);
            if (!passed) {
                all_passed = false;
            }
        }
        
        auto suite_end = std::chrono::high_resolution_clock::now();
        auto suite_duration = std::chrono::duration<double>(suite_end - suite_start);
        
        all_tests_passed_.store(all_passed);
        tests_completed_.store(true);
        
        console_separator();
        print_summary(suite_duration.count());
    }
    
    void print_summary(double suite_duration = 0.0) {
        console_section("Test Results Summary");
        
        int passed = 0;
        int failed = 0;
        int skipped = 0;
        double total_time = 0.0;
        
        // Category-based statistics
        std::map<TestCategory, int> category_passed;
        std::map<TestCategory, int> category_failed;
        std::map<TestCategory, int> category_skipped;
        std::map<TestCategory, double> category_time;
        
        for (const auto& result : test_results_) {
            total_time += result.execution_time;
            category_time[result.category] += result.execution_time;
            
            if (result.skipped) {
                skipped++;
                category_skipped[result.category]++;
            } else if (result.passed) {
                passed++;
                category_passed[result.category]++;
            } else {
                failed++;
                category_failed[result.category]++;
            }
        }
        
        console_separator();
        console_printf("Total tests: %d", (int)test_results_.size());
        console_printf("✅ Passed: %d", passed);
        console_printf("❌ Failed: %d", failed);  
        console_printf("⏭️  Skipped: %d", skipped);
        console_printf("⏱️  Total test time: %.3fs", total_time);
        
        if (suite_duration > 0.0) {
            console_printf("🕐 Suite duration: %.3fs", suite_duration);
            console_printf("📊 Test efficiency: %.1f%% (test time / suite time)", 
                (total_time / suite_duration) * 100.0);
        }
        
        // Category breakdown
        if (test_results_.size() > 5) {
            console_separator();
            console_info("Results by category:");
            for (const auto& category : enabled_categories_) {
                int cat_passed = category_passed[category];
                int cat_failed = category_failed[category];
                int cat_skipped = category_skipped[category];
                double cat_time = category_time[category];
                
                if (cat_passed + cat_failed + cat_skipped > 0) {
                    console_printf("  %s: %d✅ %d❌ %d⏭️ (%.3fs)", 
                        category_to_string(category).c_str(),
                        cat_passed, cat_failed, cat_skipped, cat_time);
                }
            }
        }
        
        // Performance analysis for performance tests
        bool has_perf_tests = false;
        double fastest_test = std::numeric_limits<double>::max();
        double slowest_test = 0.0;
        std::string fastest_name, slowest_name;
        
        for (const auto& result : test_results_) {
            if (result.category == TestCategory::PERFORMANCE && !result.skipped) {
                has_perf_tests = true;
                if (result.execution_time < fastest_test) {
                    fastest_test = result.execution_time;
                    fastest_name = result.test_name;
                }
                if (result.execution_time > slowest_test) {
                    slowest_test = result.execution_time;
                    slowest_name = result.test_name;
                }
            }
        }
        
        if (has_perf_tests) {
            console_separator();
            console_info("Performance test analysis:");
            console_printf("  ⚡ Fastest: %s (%.3fs)", fastest_name.c_str(), fastest_test);
            console_printf("  🐌 Slowest: %s (%.3fs)", slowest_name.c_str(), slowest_test);
            
            if (slowest_test > 10.0) {
                console_warning("Some performance tests are taking a long time");
                console_info("Consider optimizing or splitting long-running tests");
            }
        }
        
        if (failed > 0) {
            console_separator();
            console_error("Failed tests:");
            for (const auto& result : test_results_) {
                if (!result.passed && !result.skipped) {
                    console_printf("  ❌ %s [%s]: %s", 
                        result.test_name.c_str(), 
                        category_to_string(result.category).c_str(),
                        result.error_message.c_str());
                }
            }
        }
        
        console_separator();
        if (failed == 0 && passed > 0) {
            console_info("🎉 ALL TESTS PASSED! Runtime is working correctly.");
            if (skipped > 0) {
                console_printf("Note: %d tests were skipped due to filtering", skipped);
            }
        } else if (passed > 0 && failed > 0) {
            console_printf("⚡ MIXED RESULTS: %d passed, %d failed", passed, failed);
        } else if (passed == 0 && failed > 0) {
            console_error("💥 ALL TESTS FAILED! Check runtime configuration.");
        } else {
            console_warning("🤔 NO TESTS RUN - check filters and test discovery");
        }
    }
    
    bool all_tests_passed() const {
        return all_tests_passed_.load();
    }
    
    bool tests_completed() const {
        return tests_completed_.load();
    }

private:
    void register_test_functions() {
        console_info("Registering test assertion functions...");
        
        // Replace print to capture output
        lua_register(lua_state_, "print", lua_test_print);
        
        // Test assertion functions
        lua_register(lua_state_, "assert_equals", lua_assert_equals);
        lua_register(lua_state_, "assert_true", lua_assert_true);
        lua_register(lua_state_, "assert_false", lua_assert_false);
        lua_register(lua_state_, "assert_not_nil", lua_assert_not_nil);
        lua_register(lua_state_, "assert_nil", lua_assert_nil);
        
        console_info("Test functions registered successfully");
    }
    
    // Custom Lua print function that captures output
    static int lua_test_print(lua_State* L) {
        LuaTestRunner* runner = get_runner_instance();
        if (!runner) return 0;
        
        int n = lua_gettop(L);
        std::string output;
        
        for (int i = 1; i <= n; i++) {
            if (i > 1) output += "\t";
            
            size_t len;
            const char* s = lua_tolstring(L, i, &len);
            if (s) output += s;
            lua_pop(L, 1); // Remove string created by luaL_tolstring
        }
        
        runner->current_test_output_.push_back(output);
        
        if (runner->verbose_mode_) {
            console_printf("[TEST] %s", output.c_str());
        }
        
        return 0;
    }
    
    // Test assertion functions
    static int lua_assert_equals(lua_State* L) {
        size_t len1, len2;
        const char* expected = lua_tolstring(L, 1, &len1);
        const char* actual = lua_tolstring(L, 2, &len2);
        const char* message = luaL_optstring(L, 3, "Values should be equal");
        
        bool equals = (std::string(expected) == std::string(actual));
        
        if (!equals) {
            std::string error_msg = std::string(message) + " - Expected: '" + 
                                   expected + "', Got: '" + actual + "'";
            lua_pushstring(L, error_msg.c_str());
            lua_error(L);
        }
        
        lua_pop(L, 2); // Remove strings created by luaL_tolstring
        return 0;
    }
    
    static int lua_assert_true(lua_State* L) {
        bool value = lua_toboolean(L, 1);
        const char* message = luaL_optstring(L, 2, "Expected true");
        
        if (!value) {
            lua_pushstring(L, message);
            lua_error(L);
        }
        return 0;
    }
    
    static int lua_assert_false(lua_State* L) {
        bool value = lua_toboolean(L, 1);
        const char* message = luaL_optstring(L, 2, "Expected false");
        
        if (value) {
            lua_pushstring(L, message);
            lua_error(L);
        }
        return 0;
    }
    
    static int lua_assert_not_nil(lua_State* L) {
        bool is_nil = lua_isnil(L, 1);
        const char* message = luaL_optstring(L, 2, "Expected non-nil value");
        
        if (is_nil) {
            lua_pushstring(L, message);
            lua_error(L);
        }
        return 0;
    }
    
    static int lua_assert_nil(lua_State* L) {
        bool is_nil = lua_isnil(L, 1);
        const char* message = luaL_optstring(L, 2, "Expected nil value");
        
        if (!is_nil) {
            lua_pushstring(L, message);
            lua_error(L);
        }
        return 0;
    }
    
    // Global instance for static callbacks
    static LuaTestRunner* runner_instance_;
    static LuaTestRunner* get_runner_instance() { return runner_instance_; }
public:
    void set_as_global_instance() { runner_instance_ = this; }
};

// Static member definition
LuaTestRunner* LuaTestRunner::runner_instance_ = nullptr;

// Global test runner for runtime app mode
static std::unique_ptr<LuaTestRunner> g_test_runner;
static std::vector<std::string> g_test_files;

void run_tests_in_runtime() {
    console_section("Running Tests in Graphics Mode");
    
    // Initialize required systems
    if (!init_input_system()) {
        console_error("Failed to initialize input system for tests!");
        exit_runtime();
        return;
    }
    
    // Mark runtime as pre-initialized for Lua bindings
    lua_mark_runtime_initialized();
    
    // Run the tests
    g_test_runner->run_all_tests(g_test_files);
    
    // Wait briefly to show results
    console_info("Tests completed - waiting 3 seconds before exit");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    exit_runtime();
}

void print_usage(const char* program_name) {
    console_section("Modern Lua Test Runner Usage");
    console_printf("Usage: %s [OPTIONS] [TEST_FILES...]", program_name);
    console_separator();
    console_info("Options:");
    console_info("  -h, --help               Show this help message");
    console_info("  -v, --verbose            Enable verbose output");
    console_info("  --headless               Run without graphics (console only)");
    console_info("  --graphics               Run with graphics runtime (default)");
    console_info("  --filter PATTERN         Filter tests by regex pattern");
    console_info("  --category CATEGORY      Run only tests in specific category");
    console_info("  --exclude-category CAT   Exclude tests in specific category"); 
    console_separator();
    console_info("Categories:");
    console_info("  basic, performance, integration, graphics, console, runtime");
    console_separator();
    console_info("Examples:");
    console_printf("  %s                           # Auto-discover and run all tests", program_name);
    console_printf("  %s -v                        # Run with verbose output", program_name);
    console_printf("  %s --headless                # Run without graphics", program_name);
    console_printf("  %s --filter performance      # Run only performance tests", program_name);
    console_printf("  %s --category graphics       # Run only graphics tests", program_name);
    console_printf("  %s --exclude-category perf   # Skip performance tests", program_name);
    console_printf("  %s test_basic.lua            # Run specific test file", program_name);
}

int main(int argc, char* argv[]) {
    console_section("Modern Lua Test Runner for Abstract Runtime v2.0");
    console_info("Advanced test runner with filtering, categorization, and enhanced reporting");
    console_separator();
    
    // Parse command line arguments
    bool verbose_mode = false;
    bool headless_mode = false;
    bool show_help = false;
    std::vector<std::string> specified_tests;
    std::string filter_pattern;
    std::unordered_set<LuaTestRunner::TestCategory> enabled_categories;
    std::unordered_set<LuaTestRunner::TestCategory> excluded_categories;
    bool has_category_filter = false;
    
    // Helper to parse category string
    auto parse_category = [](const std::string& cat_str) -> LuaTestRunner::TestCategory {
        std::string lower_cat = cat_str;
        std::transform(lower_cat.begin(), lower_cat.end(), lower_cat.begin(), ::tolower);
        
        if (lower_cat == "basic") return LuaTestRunner::TestCategory::BASIC;
        if (lower_cat == "performance" || lower_cat == "perf") return LuaTestRunner::TestCategory::PERFORMANCE;
        if (lower_cat == "integration" || lower_cat == "int") return LuaTestRunner::TestCategory::INTEGRATION;
        if (lower_cat == "graphics" || lower_cat == "gfx") return LuaTestRunner::TestCategory::GRAPHICS;
        if (lower_cat == "console" || lower_cat == "con") return LuaTestRunner::TestCategory::CONSOLE;
        if (lower_cat == "runtime" || lower_cat == "rt") return LuaTestRunner::TestCategory::RUNTIME;
        
        console_warning(("Unknown category: " + cat_str).c_str());
        return LuaTestRunner::TestCategory::BASIC;
    };
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_help = true;
            break;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose_mode = true;
        } else if (arg == "--headless") {
            headless_mode = true;
        } else if (arg == "--graphics") {
            headless_mode = false;
        } else if (arg == "--filter") {
            if (i + 1 < argc) {
                filter_pattern = argv[++i];
            } else {
                console_error("--filter requires a pattern argument");
                show_help = true;
                break;
            }
        } else if (arg == "--category") {
            if (i + 1 < argc) {
                enabled_categories.insert(parse_category(argv[++i]));
                has_category_filter = true;
            } else {
                console_error("--category requires a category argument");
                show_help = true;
                break;
            }
        } else if (arg == "--exclude-category") {
            if (i + 1 < argc) {
                excluded_categories.insert(parse_category(argv[++i]));
                has_category_filter = true;
            } else {
                console_error("--exclude-category requires a category argument");
                show_help = true;
                break;
            }
        } else if (arg[0] == '-') {
            console_error(("Unknown option: " + arg).c_str());
            show_help = true;
            break;
        } else {
            // It's a test file
            specified_tests.push_back(arg);
        }
    }
    
    if (show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    // Create test runner
    auto test_runner = std::make_unique<LuaTestRunner>(verbose_mode, headless_mode);
    
    // Apply filters
    if (!filter_pattern.empty()) {
        test_runner->set_test_filter(filter_pattern);
        console_info(("Using test filter: " + filter_pattern).c_str());
    }
    
    // Apply category filters
    if (has_category_filter) {
        if (!enabled_categories.empty()) {
            test_runner->set_categories(enabled_categories);
            console_info("Running only selected categories");
        }
        
        for (const auto& excluded : excluded_categories) {
            test_runner->disable_category(excluded);
        }
        
        if (!excluded_categories.empty()) {
            console_info("Excluding selected categories");
        }
    }
    
    test_runner->set_as_global_instance();
    
    // Initialize test runner
    if (!test_runner->initialize()) {
        console_error("Failed to initialize test runner!");
        return 1;
    }
    
    // Discover test files
    std::vector<std::string> test_files;
    if (!specified_tests.empty()) {
        test_files = specified_tests;
        console_printf("Using %zu specified test files", test_files.size());
    } else {
        test_files = test_runner->discover_test_files();
    }
    
    if (test_files.empty()) {
        console_error("No test files found!");
        console_info("Place test files in lua_tests/ directory");
        console_info("Test files should start with 'test_' or end with '_test.lua'");
        return 1;
    }
    
    // Run tests
    if (headless_mode) {
        console_info("Running in headless mode (no graphics)");
        test_runner->run_all_tests(test_files);
    } else {
        console_info("Running in graphics mode with full runtime support");
        
        // Set up for runtime execution
        g_test_runner = std::move(test_runner);
        g_test_files = test_files;
        
        // Run with graphics runtime
        int result = run_runtime_with_app(SCREEN_800x600, run_tests_in_runtime);
        if (result != 0) {
            console_error("Runtime execution failed!");
            return 1;
        }
    }
    
    // Check results
    bool all_passed = headless_mode ? test_runner->all_tests_passed() : g_test_runner->all_tests_passed();
    
    console_section("Final Result");
    if (all_passed) {
        console_info("🎉 ALL TESTS PASSED! Runtime is working correctly.");
    } else {
        console_error("❌ SOME TESTS FAILED! Check output above for details.");
    }
    
    return all_passed ? 0 : 1;
}