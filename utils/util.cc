#include "util.h"
#include <cstring>
#include <vector>
#include <unistd.h>

namespace jiayou::util {
namespace {
class StringView {
	public:
		StringView(const char* str, size_t len) : beg_(str), size_(len) { }
		StringView(const char* str) : StringView(str, strlen(str)) { }
		StringView(const std::string& str) : StringView(str.c_str(), str.size()) { }
		StringView() {}

		const std::string::value_type& operator[](size_t i) const {
			return beg_[i];
		}

		size_t size() const { return size_; }
		const std::string::value_type* data() const { return beg_; }

	private:
		const std::string::value_type* beg_;
		size_t size_;
};

#if __cplusplus < 201703L
using string_view = StringView;
#else
using string_view = std::string_view;
#endif

void Extractpath(const string_view& path, size_t cur_index, std::vector<string_view>* vec) {
		auto beg = cur_index;
		for (;beg < path.size(); ++beg) {
				if (path[beg] != '/') {
						break;
				}
		}

		if (beg == path.size()) {
				return;
		}



		auto cur = beg;
		for (; cur < path.size(); ++cur) {
				if (path[cur] == '/') {
						break;
				}
		}
		string_view one(&path[beg], cur - beg);
		vec->push_back(one);
		Extractpath(path, cur, vec);
}

std::string CanonicalPath(const std::string& path) {
		std::vector<string_view> paths;
		Extractpath(path, 0, &paths);

		if (path.empty()) {
				return "";
		}

		std::vector<string_view> canonical_paths;
		for (int i = 0; i < paths.size(); ++i) {
				if (strncmp(paths[i].data(), "..", 2) == 0 && !canonical_paths.empty()) {
						canonical_paths.pop_back();
				}

				if (strncmp(paths[i].data(), ".", 1) == 0) {
						continue;
				}

				canonical_paths.push_back(paths[i]);
		}

		std::string ret;

		if (path[0] == '/') {
				ret.append("/");
		}

		for (size_t i = 0; i < canonical_paths.size(); ++i) {
				auto& p = canonical_paths[i];
				ret.append(p.data(), p.size());
				if (i != canonical_paths.size() - 1) {
						ret.append("/");
				}
		}

		return ret;
}

}

bool IsAbsolutePath(const std::string& path) {
	return !path.empty() && path[0] == '/';
}

std::string AbsolutePath(const std::string& path) {

		if (IsAbsolutePath(path)) {
			return CanonicalPath(path);
		}

		char* cwd  = get_current_dir_name();
		if (cwd == nullptr) {
				return CanonicalPath(path);
		}

		return CanonicalPath(std::string(cwd).append("/").append(path));
}

} // namespace jiayou::utils