#include "loader.hpp"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace {

struct exampleConfig {
    std::string example_name;
    int example_value;
    bool example_enabled;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(exampleConfig, example_name, example_value, example_enabled)

class TemporaryJsonFile {
public:
	TemporaryJsonFile(const std::string& testName, const std::string& contents)
		: path_(std::filesystem::temp_directory_path() / ("config_loader_" + testName + ".json")) {
		std::ofstream file(path_);
		file << contents;
	}

	~TemporaryJsonFile() {
		std::error_code error;
		std::filesystem::remove(path_, error);
	}

	const std::filesystem::path& path() const { return path_; }

private:
	std::filesystem::path path_;
};

}  // namespace

TEST(ConfigLoaderTest, LoadConfigSuccessfully) {
	const TemporaryJsonFile file(
		"success",
		R"({"example_name":"unit-test","example_value":42,"example_enabled":true})");
	exampleConfig config{};

	const auto error = ConfigLoader::LoadConfig(file.path().string(), config);

	EXPECT_FALSE(error.has_value()) << error.value_or("");
	EXPECT_EQ(config.example_name, "unit-test");
	EXPECT_EQ(config.example_value, 42);
	EXPECT_TRUE(config.example_enabled);
}

TEST(ConfigLoaderTest, ReturnsErrorForMissingFile) {
	const auto path = std::filesystem::temp_directory_path() / "config_loader_missing.json";
	std::filesystem::remove(path);
	exampleConfig config{};

	const auto error = ConfigLoader::LoadConfig(path.string(), config);

	ASSERT_TRUE(error.has_value());
	EXPECT_NE(error->find("Unable to open configuration file"), std::string::npos);
}

TEST(ConfigLoaderTest, InvalidJsonDoesNotModifyConfig) {
	const TemporaryJsonFile file("invalid_json", "{ invalid json }");
	exampleConfig config{"original", 7, false};

	const auto error = ConfigLoader::LoadConfig(file.path().string(), config);

	ASSERT_TRUE(error.has_value());
	EXPECT_NE(error->find("Unable to parse configuration file"), std::string::npos);
	EXPECT_EQ(config.example_name, "original");
	EXPECT_EQ(config.example_value, 7);
	EXPECT_FALSE(config.example_enabled);
}