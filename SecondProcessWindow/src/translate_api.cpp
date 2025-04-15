#include "translate_api.h"
#include <algorithm>
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json/json.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include "spdlog/spdlog.h"

using namespace std;

string read_file(const string &path)
{
    ifstream file(path);
    stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

string sha256Hex(const string &data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)data.c_str(), data.size(), hash);
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    return ss.str();
}

string hmac_sha256(const string &key, const string &data)
{
    unsigned char *result;
    unsigned int len = SHA256_DIGEST_LENGTH;
    result = HMAC(EVP_sha256(), key.data(), key.length(),
                  (unsigned char *)data.data(), data.length(), NULL, NULL);
    return string((char *)result, len);
}

string hexEncode(const string &data)
{
    stringstream ss;
    for (unsigned char c : data)
        ss << hex << setw(2) << setfill('0') << (int)c;
    return ss.str();
}

string getDateString(time_t t)
{
    tm gmTime;
    gmtime_s(&gmTime, &t);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &gmTime);
    return string(buf);
}

string intToString(int64_t n)
{
    stringstream ss;
    ss << n;
    return ss.str();
}

size_t writeCallback(void *contents, size_t size, size_t nmemb, string *output)
{
    size_t totalSize = size * nmemb;
    output->append((char *)contents, totalSize);
    return totalSize;
}

std::string tencent_translate(const string &text, const string &source = "zh",
                              const string &target = "en")
{
    string secret_id = read_file("keys/id.txt");
    string secret_key = read_file("keys/key.txt");

    string service = "tmt";
    string host = "tmt.tencentcloudapi.com";
    string region = "ap-guangzhou";
    string action = "TextTranslate";
    string version = "2018-03-21";
    string algorithm = "TC3-HMAC-SHA256";
    time_t timestamp = time(NULL);
    string date = getDateString(timestamp);

    // payload
    string payload =
        R"({"SourceText": ")" + text + R"(", "Source": ")" + source +
        R"(", "Target": ")" + target +
        R"(", "ProjectId": 0})"; // "SourceText": "你说什么呢？", "Source":
                                 // "zh", "Target": "en", "ProjectId": 0})";

    string lower_action = action;
    std::transform(lower_action.begin(), lower_action.end(),
                   lower_action.begin(), ::tolower);

    // Step 1: Canonical request
    string canonical_request = "POST\n/\n\n"
                               "content-type:application/json; charset=utf-8\n"
                               "host:" +
                               host + "\n" + "x-tc-action:" + lower_action +
                               "\n\n"
                               "content-type;host;x-tc-action\n" +
                               sha256Hex(payload);

    // Step 2: String to sign
    string credential_scope = date + "/" + service + "/tc3_request";
    string hashed_canonical_request = sha256Hex(canonical_request);
    string string_to_sign = algorithm + "\n" + intToString(timestamp) + "\n" +
                            credential_scope + "\n" + hashed_canonical_request;

    // Step 3: Signature
    string secret_date = hmac_sha256("TC3" + secret_key, date);
    string secret_service = hmac_sha256(secret_date, service);
    string secret_signing = hmac_sha256(secret_service, "tc3_request");
    string signature = hexEncode(hmac_sha256(secret_signing, string_to_sign));

    // Step 4: Authorization header
    string authorization =
        algorithm + " Credential=" + secret_id + "/" + credential_scope +
        ", SignedHeaders=content-type;host;x-tc-action, Signature=" + signature;

    // Step 5: Send request using libcurl
    CURL *curl = curl_easy_init();

    std::string result = "empty";
    if (curl)
    {
        string response;
        string url = "https://tmt.tencentcloudapi.com";
        struct curl_slist *headers = NULL;

        headers = curl_slist_append(
            headers, ("Content-Type: application/json; charset=utf-8"));
        headers = curl_slist_append(headers, ("Host: " + host).c_str());
        headers =
            curl_slist_append(headers, ("X-TC-Action: " + action).c_str());
        headers = curl_slist_append(
            headers, ("X-TC-Timestamp: " + intToString(timestamp)).c_str());
        headers =
            curl_slist_append(headers, ("X-TC-Version: " + version).c_str());
        headers =
            curl_slist_append(headers, ("X-TC-Region: " + region).c_str());
        headers = curl_slist_append(
            headers, ("Authorization: " + authorization).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK)
        {
            // cout << "返回结果：" << response << endl;
            spdlog::info("返回结果: {}", response);
            Json::Value root;
            Json::CharReaderBuilder reader;
            istringstream s(response);
            string errs;
            if (Json::parseFromStream(reader, s, &root, &errs))
            {
                if (root.isMember("Response") &&
                    root["Response"].isMember("TargetText"))
                {
                    // cout << "翻译结果: " <<
                    // root["Response"]["TargetText"].asString() << endl;
                    result = root["Response"]["TargetText"].asString();
                }
            }
            else
            {
                // cerr << "JSON 解析失败: " << errs << endl;
                spdlog::info("JSON 解析失败: {}", errs);
            }
        }
        else
        {
            // cerr << "请求失败: " << curl_easy_strerror(res) << endl;
            spdlog::info("请求失败: {}", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    return result;
}