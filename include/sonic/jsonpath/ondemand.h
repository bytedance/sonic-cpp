
#include <tuple>
#include "sonic/dom/parser.h"
#include "sonic/dom/generic_document.h"
#include "sonic/jsonpath/jsonpath.h"
#include "sonic/jsonpath/dump.h"

namespace sonic_json {

struct JsonPathRawResult {
  std::vector<StringView> raw;
  SonicError error;
};

sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPathOnDemand(
    StringView json, StringView jsonpath) {

    internal::SkipScanner2 scan;
      // serialize the dom
    JsonPathResult<Node> result;

    scan.data_ = reinterpret_cast<const uint8_t*>(json.data());
    scan.len_ = json.size();
    JsonPathRawResult ret = {
        .raw = {},
        .error = kErrorNone,
    };
    internal::JsonPath path;

    // padding some buffers
    std::string pathpadd = internal::paddingJsonPath(jsonpath);
    if (!path.Parse(pathpadd)) {
        ret.raw.clear();
        return std::make_tuple("", kUnsupportedJsonPath);
    }

    if (path[0].is_root() && path.size() == 1) {
        ret.raw.push_back(json);
    } else {
        ret.error = scan.getJsonPath(path, 1, ret.raw);
        if (ret.error != kErrorNone) {
            ret.raw.clear();
            return std::make_tuple("", ret.error);
        }
    }

    // parse the raw json into dom
    std::vector<Document> doms(ret.raw.size());
    for (auto& r : ret.raw) {
      Document dom;
      dom.Parse(r);
      if (dom.HasParseError()) {
        ret.raw.clear();
        return std::make_tuple("", dom.GetParseError());;
      }
      doms.emplace_back(std::move(dom));
    }



    for (size_t i = 0; i < doms.size(); i++) {
       result.nodes.push_back(&doms[i]);
    }

    return internal::Serialize(result);
}

}