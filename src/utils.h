#pragma once

namespace monolm {
	class Utils {
	public:
		Utils() = delete;

		static std::string GetEnvVariable(const char* varName);
		static bool SetEnvVariable(const char* varName, const char* value);

		static std::string ReadText(const fs::path& filepath) {
			std::ifstream istream(filepath, std::ios::binary);
			if (!istream.is_open())
				return {};
			istream.unsetf(std::ios::skipws);
			return { std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>() };
		}

		template<typename T>
		static std::vector<T> ReadBytes(const fs::path& filepath) {
			std::ifstream istream(filepath, std::ios::binary);
			if (!istream.is_open())
				return {};
			return { std::istreambuf_iterator<char>(istream), std::istreambuf_iterator<char>() };
		}

		static std::vector<std::string_view> Split(std::string_view strv, std::string_view delims) {
			std::vector<std::string_view> output;
			size_t first = 0;

			while (first < strv.size()) {
				const size_t second = strv.find_first_of(delims, first);

				if (first != second)
					output.emplace_back(strv.substr(first, second-first));

				if (second == std::string_view::npos)
					break;

				first = second + 1;
			}

			return output;
		}
	};
}