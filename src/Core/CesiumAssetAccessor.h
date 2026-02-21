#pragma once

#include "thirdparty.h"
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief 实现 Cesium 资源的请求解析（响应包的抽象包装类）
 */
class CesiumAssetResponse : public CesiumAsync::IAssetResponse {
public:
    CesiumAssetResponse(uint16_t statusCode, const std::string& contentType,
                        const CesiumAsync::HttpHeaders& headers,
                        std::vector<uint8_t> data);

    ~CesiumAssetResponse() override = default;

    uint16_t statusCode() const override;
    std::string contentType() const override;
    const CesiumAsync::HttpHeaders& headers() const override;
    gsl::span<const std::byte> data() const override;

private:
    uint16_t m_statusCode;
    std::string m_contentType;
    CesiumAsync::HttpHeaders m_headers;
    std::vector<uint8_t> m_data;
};

/**
 * @brief 实现 Cesium 资源的请求调用（请求包的抽象包装类）
 */
class CesiumAssetRequest : public CesiumAsync::IAssetRequest {
public:
    CesiumAssetRequest(const std::string& method, const std::string& url,
                       const CesiumAsync::HttpHeaders& headers,
                       std::unique_ptr<CesiumAssetResponse> response);

    ~CesiumAssetRequest() override = default;

    const std::string& method() const override;
    const std::string& url() const override;
    const CesiumAsync::HttpHeaders& headers() const override;
    const CesiumAsync::IAssetResponse* response() const override;

private:
    std::string m_method;
    std::string m_url;
    CesiumAsync::HttpHeaders m_headers;
    std::unique_ptr<CesiumAssetResponse> m_response;
};

/**
 * @brief Cesium 资源访问器，提供对网络瓦片 (http(s)) 或本地文件系统的异步拉取能力
 */
class CesiumAssetAccessor : public CesiumAsync::IAssetAccessor {
public:
    /**
     * @brief 构造网络资源获取器，支持代理设定
     * @param proxyHost 自定义代理的IP地址或域名（若为空则直连）
     * @param proxyPort 自定义代理的端口
     */
    explicit CesiumAssetAccessor(const std::string& proxyHost = "", int proxyPort = -1);

    void setCachePath(const std::string& path) { m_cachePath = path; }

    ~CesiumAssetAccessor() override = default;

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> get(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& url,
        const std::vector<THeader>& headers) override;

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& verb,
        const std::string& url,
        const std::vector<THeader>& headers,
        const gsl::span<const std::byte>& contentPayload) override;

    void tick() noexcept override;

private:
    std::string m_proxyHost;
    int m_proxyPort;
    std::string m_cachePath;

    std::shared_ptr<CesiumAsync::IAssetRequest> executeRequest(
        const std::string& verb,
        const std::string& url,
        const std::vector<THeader>& headers,
        const gsl::span<const std::byte>& contentPayload);

    std::string extractContentType(const httplib::Headers& headers);
};
