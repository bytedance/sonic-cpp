/*
 * Copyright 2022 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define SONIC_SPARK_FORMAT

#include <gtest/gtest.h>

#include "sonic/sonic.h"

namespace {

using namespace sonic_json;

// void TestOk(const std::string json, const std::string path,
//             const std::string expect) {
//   auto got = GetByJsonPath(json, path);
//   ASSERT_EQ(std::get<0>(got), expect)
//       << "json: " << json << ", path: " << path
//       << ", error: " << ErrorMsg(std::get<1>(got));
// }

#define TestOk(json, path, expect)                    \
  {                                                   \
    auto got = GetByJsonPathOnDemand(json, path);     \
    EXPECT_EQ(std::get<0>(got), expect)               \
        << "json: " << json << ", path: " << path     \
        << ", error: " << ErrorMsg(std::get<1>(got)); \
  }

// void TestFail(const std::string json, const std::string path) {
//   // ASSERT_NE(std::get<1>(GetByJsonPathOnDemand(json, path)), kErrorNone)
//   //     << "json: " << json << ", path: " << path;
// }
/**
// #define TestFail(json, path) {\
// EXPECT_NE(std::get<1>(GetByJsonPathOnDemand(json, path)), kErrorNone)\
// << "json: " << json << ", path: " << path;\
//   }
 **/

TEST(JsonTuple, Basic) {
  auto json = R"({
        "a": 1,
        "b": [0,1,2,3],
        "c": {"33": 123}
        })";
  // StringView json, std::vector<StringView> keys
  auto result = JsonTupleWithCodeGen(json, {"b"}, true);
  std::vector<std::optional<std::string>> expected = {R"([0,1,2,3])"};
  EXPECT_EQ(result, expected);
}
TEST(JsonTuple, sparkCornerCase) {
  std::string json = R"({"1.a": "b"})";
  std::vector<std::string_view> paths{"1.a"};
  std::vector<std::optional<std::string>> expected = {R"(b)"};
  auto result = JsonTupleWithCodeGen(json, paths, true);
  EXPECT_EQ(result, expected);
}
TEST(JsonTuple, escapeQuote) {
  std::string json = R"({"1.a": "{\"options\"}")";
  std::vector<std::string_view> paths{"1.a"};
  std::vector<std::optional<std::string>> expected = {R"({"options"})"};
  auto result = JsonTupleWithCodeGen(json, paths, true);
  EXPECT_EQ(result, expected);
}
TEST(JsonTuple, japanese) {
  std::string json = R"json(
    {
      "id": 903487807,
      "id_str": "903487807",
      "name": "RT&ファボ魔のむっつんさっm",
      "screen_name": "yuttari1998",
      "location": "関西    ↓詳しいプロ↓",
      "description": "無言フォローはあまり好みません ゲームと動画が好きですシモ野郎ですがよろしく…最近はMGSとブレイブルー、音ゲーをプレイしてます",
      "url": "http://t.co/Yg9e1Fl8wd",
      "entities": {
        "url": {
          "urls": [
            {
              "url": "http://t.co/Yg9e1Fl8wd",
              "expanded_url": "http://twpf.jp/yuttari1998",
              "display_url": "twpf.jp/yuttari1998",
              "indices": [
                0,
                22
              ]
            }
          ]
        },
        "description": {
          "urls": []
        }
      },
      "protected": false,
      "followers_count": 95,
      "friends_count": 158,
      "listed_count": 1,
      "created_at": "Thu Oct 25 08:27:13 +0000 2012",
      "favourites_count": 3652,
      "utc_offset": null,
      "time_zone": null,
      "geo_enabled": false,
      "verified": false,
      "statuses_count": 10276,
      "lang": "ja",
      "contributors_enabled": false,
      "is_translator": false,
      "is_translation_enabled": false,
      "profile_background_color": "C0DEED",
      "profile_background_image_url": "http://abs.twimg.com/images/themes/theme1/bg.png",
      "profile_background_image_url_https": "https://abs.twimg.com/images/themes/theme1/bg.png",
      "profile_background_tile": false,
      "profile_image_url": "http://pbs.twimg.com/profile_images/500268849275494400/AoXHZ7Ij_normal.jpeg",
      "profile_image_url_https": "https://pbs.twimg.com/profile_images/500268849275494400/AoXHZ7Ij_normal.jpeg",
      "profile_banner_url": "https://pbs.twimg.com/profile_banners/903487807/1409062272",
      "profile_link_color": "0084B4",
      "profile_sidebar_border_color": "C0DEED",
      "profile_sidebar_fill_color": "DDEEF6",
      "profile_text_color": "333333",
      "profile_use_background_image": true,
      "default_profile": true,
      "default_profile_image": false,
      "following": false,
      "follow_request_sent": false,
      "notifications": false
    }
  )json";
  std::vector<std::string_view> paths{
      "id_str",
      "id",
      "location",
      "description",
      "entities.url.urls[0].indices[1]",
  };
  auto result = JsonTupleWithCodeGen(json, paths, true);
  std::vector<std::optional<std::string>> expected = {
      "903487807", "903487807", "関西    ↓詳しいプロ↓",
      "無言フォローはあまり好みません "
      "ゲームと動画が好きですシモ野郎ですがよろしく…最近はMGSとブレイブルー、音"
      "ゲーをプレイしてます",
      std::nullopt};

  EXPECT_EQ(result, expected);
  result = JsonTupleWithCodeGen(json, paths, false);
  EXPECT_EQ(result, expected);
}

TEST(JsonTuple, invalidValue) {
  std::string json = "{\"a\":1,\"b\":2c}";
  std::vector<std::string_view> paths{"a", "b"};
  std::vector<std::optional<std::string>> expected = {"1", std::nullopt};

  auto result = JsonTupleWithCodeGen(json, paths, true);
  EXPECT_EQ(result, expected);
  expected = {std::nullopt, std::nullopt};
  result = JsonTupleWithCodeGen(json, paths, false);
  EXPECT_EQ(result, expected);
}

TEST(JsonTuple, malformed) {
  std::string json = "{\"a\":1,\"b\":2, c}";
  std::vector<std::string_view> paths{"a", "b", "c", "d"};
  std::vector<std::optional<std::string>> expected = {"1", "2", std::nullopt,
                                                      std::nullopt};
  std::vector<std::optional<std::string>> expected2 = {
      std::nullopt, std::nullopt, std::nullopt, std::nullopt};

  auto result = JsonTupleWithCodeGen(json, paths, true);
  EXPECT_EQ(result, expected);

  result = JsonTupleWithCodeGen(json, paths, false);
  EXPECT_EQ(result, expected2);
}

}  // namespace
