#include "CesiumAssetAccessor.h"
#include "../Bridge/Log.h"
#include "../Bridge/ResourceLoader.h"
#include <algorithm>

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

CesiumAssetAccessor::CesiumAssetAccessor(const std::string& proxyHost, int proxyPort)
    : m_proxyHost(proxyHost), m_proxyPort(proxyPort) {}

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

    if (url.find("file://") == 0 || url.find("http") != 0) {
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
        CesiumAsync::HttpHeaders requestHeadersMap;
        for (const auto& h : headers) {
            if (h.first == "Accept-Encoding") continue;

            httpHeaders.insert({h.first, h.second});
            requestHeadersMap[h.first] = h.second;
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

            std::string snippet = res->body.substr(0, std::min<size_t>(res->body.size(), 100));
            NX_LOG_INFO("Cesium HTTP: {} -> Status: {}, Type: {}, Enc: {}, Size: {}, Prefix: {}",
                url, statusCode, contentType, contentEncoding, res->body.size(), snippet);
        } else {
            statusCode = 0;
            NX_LOG_ERROR("HTTP Request failed: {} - Err: {}", url, httplib::to_string(res.error()));
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
