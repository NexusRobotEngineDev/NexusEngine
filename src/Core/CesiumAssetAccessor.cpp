#include "CesiumAssetAccessor.h"
#include "../Bridge/Log.h"
#include "../Bridge/ResourceLoader.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

CesiumAssetResponse::CesiumAssetResponse(uint16_t statusCode, const std::string& contentType,
                                         const CesiumAsync::HttpHeaders& headers,
                                         std::vector<uint8_t> data)
    : m_statusCode(statusCode), m_contentType(contentType), m_headers(headers), m_data(std::move(data)) {}

uint16_t CesiumAssetResponse::statusCode() const { return m_statusCode; }

std::string CesiumAssetResponse::contentType() const { return m_contentType; }

const CesiumAsync::HttpHeaders& CesiumAssetResponse::headers() const { return m_headers; }

gsl::span<const std::byte> CesiumAssetResponse::data() const {
    return gsl::span<const std::byte>(reinterpret_cast<const std::byte*>(m_data.data()), m_data.size());
}

CesiumAssetRequest::CesiumAssetRequest(const std::string& method, const std::string& url,
                                       const CesiumAsync::HttpHeaders& headers,
                                       std::unique_ptr<CesiumAssetResponse> response)
    : m_method(method), m_url(url), m_headers(headers), m_response(std::move(response)) {}

const std::string& CesiumAssetRequest::method() const { return m_method; }

const std::string& CesiumAssetRequest::url() const { return m_url; }

const CesiumAsync::HttpHeaders& CesiumAssetRequest::headers() const { return m_headers; }

const CesiumAsync::IAssetResponse* CesiumAssetRequest::response() const { return m_response.get(); }

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winreg.h>

static void autoDetectProxy(std::string& host, int& port) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD enableProxy = 0;
        DWORD dataSize = sizeof(enableProxy);
        if (RegQueryValueExA(hKey, "ProxyEnable", nullptr, nullptr, reinterpret_cast<LPBYTE>(&enableProxy), &dataSize) == ERROR_SUCCESS) {
            if (enableProxy) {
                char proxyServer[512] = {0};
                DWORD proxyServerSize = sizeof(proxyServer);
                if (RegQueryValueExA(hKey, "ProxyServer", nullptr, nullptr, reinterpret_cast<LPBYTE>(proxyServer), &proxyServerSize) == ERROR_SUCCESS) {
                    std::string proxyStr(proxyServer);

                    auto parseHostPort = [&](const std::string& str) {
                        size_t colon = str.find_last_of(':');
                        if (colon != std::string::npos && colon < str.size() - 1) {
                            host = str.substr(0, colon);
                            try {
                                port = std::stoi(str.substr(colon + 1));
                            } catch (...) {
                                port = -1;
                                host = "";
                            }
                        }
                    };

                    if (proxyStr.find('=') != std::string::npos) {
                        size_t httpPos = proxyStr.find("http=");
                        if (httpPos != std::string::npos) {
                            size_t endPos = proxyStr.find(';', httpPos);
                            std::string httpProxy = proxyStr.substr(httpPos + 5, endPos == std::string::npos ? std::string::npos : endPos - (httpPos + 5));
                            parseHostPort(httpProxy);
                        } else {
                            size_t eqPos = proxyStr.find('=');
                            size_t endPos = proxyStr.find(';');
                            std::string proxy = proxyStr.substr(eqPos + 1, endPos == std::string::npos ? std::string::npos : endPos - (eqPos + 1));
                            parseHostPort(proxy);
                        }
                    } else {
                        parseHostPort(proxyStr);
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }
}
#else
#include <cstdlib>

static void autoDetectProxy(std::string& host, int& port) {
    const char* envProxy = std::getenv("all_proxy");
    if (!envProxy) envProxy = std::getenv("http_proxy");
    if (!envProxy) envProxy = std::getenv("https_proxy");
    if (!envProxy) envProxy = std::getenv("HTTP_PROXY");
    if (!envProxy) envProxy = std::getenv("HTTPS_PROXY");

    if (envProxy) {
        std::string proxyStr(envProxy);

        size_t prefixPos = proxyStr.find("://");
        if (prefixPos != std::string::npos) {
            proxyStr = proxyStr.substr(prefixPos + 3);
        }

        if (!proxyStr.empty() && proxyStr.back() == '/') {
            proxyStr.pop_back();
        }

        size_t colon = proxyStr.find_last_of(':');
        if (colon != std::string::npos && colon < proxyStr.size() - 1) {
            host = proxyStr.substr(0, colon);
            try {
                port = std::stoi(proxyStr.substr(colon + 1));
            } catch (...) {
                port = -1;
                host = "";
            }
        }
    }
}
#endif

CesiumAssetAccessor::CesiumAssetAccessor(const std::string& proxyHost, int proxyPort)
    : m_proxyHost(proxyHost), m_proxyPort(proxyPort) {
    if (m_proxyHost.empty() && m_proxyPort <= 0) {
        autoDetectProxy(m_proxyHost, m_proxyPort);
        if (!m_proxyHost.empty()) {
            NX_LOG_INFO("Auto-detected System Proxy: {}:{}", m_proxyHost, m_proxyPort);
        }
    }
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> CesiumAssetAccessor::get(
    const CesiumAsync::AsyncSystem& asyncSystem, const std::string& url, const std::vector<THeader>& headers) {
    return request(asyncSystem, "GET", url, headers, {});
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> CesiumAssetAccessor::request(
    const CesiumAsync::AsyncSystem& asyncSystem, const std::string& verb, const std::string& url,
    const std::vector<THeader>& headers, const gsl::span<const std::byte>& contentPayload) {

    auto promise = asyncSystem.createPromise<std::shared_ptr<CesiumAsync::IAssetRequest>>();

    std::vector<uint8_t> payloadData;
    if (!contentPayload.empty()) {
        payloadData.assign(reinterpret_cast<const uint8_t*>(contentPayload.data()),
                           reinterpret_cast<const uint8_t*>(contentPayload.data() + contentPayload.size()));
    }

    asyncSystem.runInWorkerThread([this, promise, verb, url, headers, payload = std::move(payloadData)]() mutable {
        try {
            auto req = executeRequest(verb, url, headers, gsl::span<const std::byte>(
                reinterpret_cast<const std::byte*>(payload.data()), payload.size()));
            promise.resolve(std::move(req));
        } catch (const std::exception& e) {
            NX_LOG_ERROR("Cesium Asset Accessor Exception in worker thread: url={}, error={}", url, e.what());
            promise.reject(std::runtime_error(std::string("CesiumAssetAccessor: ") + e.what()));
        } catch (...) {
            NX_LOG_ERROR("Cesium Asset Accessor Unknown Exception in worker thread: url={}", url);
            promise.reject(std::runtime_error("CesiumAssetAccessor: Unknown Exception"));
        }
    });

    return promise.getFuture();
}

std::shared_ptr<CesiumAsync::IAssetRequest> CesiumAssetAccessor::executeRequest(
    const std::string& verb, const std::string& url, const std::vector<THeader>& headers,
    const gsl::span<const std::byte>& contentPayload) {

    CesiumAsync::HttpHeaders responseHeaders;
    std::string contentType = "application/octet-stream";
    std::vector<uint8_t> responseData;
    uint16_t statusCode = 0;

    std::string headerStr = "";
    for (const auto& header : headers) {
        headerStr += " [" + header.first + ": " + header.second + "]";
    }
    NX_LOG_INFO("CesiumAssetAccessor: executeRequest url={} headers_count={} headers={}", url, headers.size(), headerStr);

    if (url.find("file://") == 0 || (url.find("http") != 0 && url.find("https") != 0)) {
        std::string localPath = url;
        if (url.find("file://") == 0) {
            localPath = url.substr(7);
#ifdef _WIN32
            if (localPath.size() > 2 && localPath[0] == '/' && localPath[2] == ':') {
                localPath = localPath.substr(1);
            }
#endif
        }

        auto fileStatus = Nexus::ResourceLoader::loadBinaryFile(localPath);
        if (fileStatus.ok()) {
            responseData = std::move(fileStatus.value());
            statusCode = 200;
        } else {
            statusCode = 404;
        }

    } else {
        std::string cacheKey;
        std::vector<THeader> cleanHeaders;
        cleanHeaders.reserve(headers.size());

        for (const auto& h : headers) {
            std::string key = h.first;
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            if (key == "x-cesium-cache-key") {
                cacheKey = h.second;
            } else {
                cleanHeaders.push_back(h);
            }
        }

        bool isJson = (url.find(".json") != std::string::npos) || (url.find("endpoint") != std::string::npos);

        std::string cacheFilePath;
        if (!m_cachePath.empty()) {
            std::string cleanUrl = url;
            size_t protoPos = cleanUrl.find("://");
            if (protoPos != std::string::npos) {
                cleanUrl = cleanUrl.substr(protoPos + 3);
            }
            size_t queryPos = cleanUrl.find('?');
            if (queryPos != std::string::npos) {
                cleanUrl = cleanUrl.substr(0, queryPos);
            }

            std::string extension = "bin";

            std::string subDirStr;
            std::string fileStr;
            size_t fileSlash = cleanUrl.find_last_of('/');
            if (fileSlash != std::string::npos) {
                subDirStr = cleanUrl.substr(0, fileSlash);
                fileStr = cleanUrl.substr(fileSlash + 1);
            } else {
                fileStr = cleanUrl;
            }

            auto lastDot = fileStr.find_last_of('.');
            if (lastDot != std::string::npos) {
                extension = fileStr.substr(lastDot + 1);
            }


            std::replace(subDirStr.begin(), subDirStr.end(), ':', '_');

            std::filesystem::path dirPath = std::filesystem::path(m_cachePath);
            if (!subDirStr.empty()) {
                dirPath /= subDirStr;
            }

            if (!cacheKey.empty()) {
                size_t bboxHash = std::hash<std::string>{}(cacheKey);
                cacheFilePath = (dirPath / ("bbox_" + std::to_string(bboxHash) + "." + extension)).string();
            } else {
                if (fileStr.empty() || fileStr == "unknown" || fileStr.find("endpoint") != std::string::npos) {
                    cacheFilePath = (dirPath / ("url_" + std::to_string(std::hash<std::string>{}(cleanUrl)) + "." + extension)).string();
                } else {
                    cacheFilePath = (dirPath / fileStr).string();
                }
            }
        }

        bool loadedFromCache = false;

        if (!cacheFilePath.empty() && std::filesystem::exists(cacheFilePath)) {
            if (m_offlineMode || (!isJson)) {
                auto fileStatus = Nexus::ResourceLoader::loadBinaryFile(cacheFilePath);
                if (fileStatus.ok()) {
                    responseData = std::move(fileStatus.value());
                    statusCode = 200;
                    loadedFromCache = true;

                    if (url.find(".glb") != std::string::npos || url.find(".gltf") != std::string::npos) {
                        contentType = "model/gltf-binary";
                    } else if (url.find(".b3dm") != std::string::npos) {
                        contentType = "application/octet-stream";
                    } else if (isJson) {
                        contentType = "application/json";
                    }
                    responseHeaders["Content-Type"] = contentType;
                }
            }
        }

        if (!loadedFromCache) {
            auto pos = url.find("://");
            auto pathPos = url.find('/', pos + 3);
            std::string host = url.substr(0, pathPos);
            std::string path = pathPos == std::string::npos ? "/" : url.substr(pathPos);

            httplib::Client client(host.c_str());
            if (!m_proxyHost.empty() && m_proxyPort > 0) {
                client.set_proxy(m_proxyHost.c_str(), m_proxyPort);
            }
            client.set_follow_location(true);

            httplib::Headers httpHeaders;
            for (const auto& h : cleanHeaders) {
                if (h.first == "Accept-Encoding") continue;
                httpHeaders.insert({h.first, h.second});
            }

            auto res = (verb == "GET")
                ? client.Get(path.c_str(), httpHeaders)
                : client.Post(path.c_str(), httpHeaders, reinterpret_cast<const char*>(contentPayload.data()),
                              contentPayload.size(), "application/octet-stream");

            if (res) {
                statusCode = static_cast<uint16_t>(res->status);
                std::string contentEncoding = "";
                for (const auto& header : res->headers) {
                    responseHeaders[header.first] = header.second;
                    if (header.first == "Content-Encoding") {
                        contentEncoding = header.second;
                    }
                }
                contentType = extractContentType(res->headers);
                responseData.assign(res->body.begin(), res->body.end());

                if (!cacheFilePath.empty() && statusCode >= 200 && statusCode < 300) {
                    try {
                        std::filesystem::path cp(cacheFilePath);
                        std::filesystem::create_directories(cp.parent_path());
                        std::ofstream ofs(cacheFilePath, std::ios::binary);
                        if (ofs) {
                            ofs.write(reinterpret_cast<const char*>(responseData.data()), responseData.size());
                        }
                    } catch (...) {
                        NX_LOG_WARN("Failed to write to cache: {}", cacheFilePath);
                    }
                }

            } else {
                statusCode = 0;
                NX_LOG_ERROR("HTTP Request failed: {} - Err: {}", url, httplib::to_string(res.error()));
            }
        }
    }

    auto response = std::make_unique<CesiumAssetResponse>(statusCode, contentType, responseHeaders, std::move(responseData));

    CesiumAsync::HttpHeaders reqHeadersMap;
    for (const auto& h : headers) {
        reqHeadersMap[h.first] = h.second;
    }

    return std::make_shared<CesiumAssetRequest>(verb, url, reqHeadersMap, std::move(response));
}

std::string CesiumAssetAccessor::extractContentType(const httplib::Headers& headers) {
    auto it = headers.find("Content-Type");
    if (it != headers.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

void CesiumAssetAccessor::tick() noexcept {}
