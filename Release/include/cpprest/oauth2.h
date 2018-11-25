/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: Oauth 2.0
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef CASA_OAUTH2_H
#define CASA_OAUTH2_H

#include "cpprest/http_msg.h"
#include "cpprest/details/web_utilities.h"

namespace web
{
namespace http
{
namespace client
{
    // Forward declaration to avoid circular include dependency.
    class http_client_config;
}

/// oAuth 2.0 library.
namespace oauth2
{
namespace details
{

class oauth2_handler;

// Constant strings for OAuth 2.0.
typedef utility::string_t oauth2_string;
class oauth2_strings
{
public:
#define _OAUTH2_STRINGS
#define DAT(a_, b_) _ASYNCRTIMP static const oauth2_string a_;
#include "cpprest/details/http_constants.dat"
#undef _OAUTH2_STRINGS
#undef DAT
};

} // namespace web::http::oauth2::details

/// oAuth functionality is currently in beta.
namespace experimental
{

/// <summary>
/// Exception type for OAuth 2.0 errors.
/// </summary>
class oauth2_exception : public std::exception
{
public:
    oauth2_exception(utility::string_t msg) : m_msg(utility::conversions::to_utf8string(std::move(msg))) {}
    ~oauth2_exception() CPPREST_NOEXCEPT {}
    const char* what() const CPPREST_NOEXCEPT { return m_msg.c_str(); }

private:
    std::string m_msg;
};

/// <summary>
/// OAuth 2.0 token and associated information.
/// </summary>
class oauth2_token
{
public:

    /// <summary>
    /// Value for undefined expiration time in expires_in().
    /// </summary>
    enum { undefined_expiration = -1 };

    oauth2_token(utility::string_t access_token=utility::string_t()) :
        m_access_token(std::move(access_token)),
        m_expires_in(undefined_expiration)
    {}

    /// <summary>
    /// Get access token validity state.
    /// If true, access token is a valid.
    /// </summary>
    /// <returns>Access token validity state.</returns>
    bool is_valid_access_token() const { return !access_token().empty(); }

    /// <summary>
    /// Get access token.
    /// </summary>
    /// <returns>Access token string.</returns>
    const utility::string_t& access_token() const { return m_access_token; }
    /// <summary>
    /// Set access token.
    /// </summary>
    /// <param name="access_token">Access token string to set.</param>
    void set_access_token(utility::string_t access_token) { m_access_token = std::move(access_token); }

    /// <summary>
    /// Get refresh token.
    /// </summary>
    /// <returns>Refresh token string.</returns>
    const utility::string_t& refresh_token() const { return m_refresh_token; }
    /// <summary>
    /// Set refresh token.
    /// </summary>
    /// <param name="refresh_token">Refresh token string to set.</param>
    void set_refresh_token(utility::string_t refresh_token) { m_refresh_token = std::move(refresh_token); }

    /// <summary>
    /// Get token type.
    /// </summary>
    /// <returns>Token type string.</returns>
    const utility::string_t& token_type() const { return m_token_type; }
    /// <summary>
    /// Set token type.
    /// </summary>
    /// <param name="token_type">Token type string to set.</param>
    void set_token_type(utility::string_t token_type) { m_token_type = std::move(token_type); }

    /// <summary>
    /// Get token scope.
    /// </summary>
    /// <returns>Token scope string.</returns>
    const utility::string_t& scope() const { return m_scope; }
    /// <summary>
    /// Set token scope.
    /// </summary>
    /// <param name="scope">Token scope string to set.</param>
    void set_scope(utility::string_t scope) { m_scope = std::move(scope); }

    /// <summary>
    /// Get the lifetime of the access token in seconds.
    /// For example, 3600 means the access token will expire in one hour from
    /// the time when access token response was generated by the authorization server.
    /// Value of undefined_expiration means expiration time is either
    /// unset or that it was not returned by the server with the access token.
    /// </summary>
    /// <returns>Lifetime of the access token in seconds or undefined_expiration if not set.</returns>
    int64_t expires_in() const { return m_expires_in; }
    /// <summary>
    /// Set lifetime of access token (in seconds).
    /// </summary>
    /// <param name="expires_in">Lifetime of access token in seconds.</param>
    void set_expires_in(int64_t expires_in) { m_expires_in = expires_in; }

private:
    utility::string_t m_access_token;
    utility::string_t m_refresh_token;
    utility::string_t m_token_type;
    utility::string_t m_scope;
    int64_t m_expires_in;
};

/// <summary>
/// OAuth 2.0 configuration.
///
/// Encapsulates functionality for:
/// -  Authenticating requests with an access token.
/// -  Performing the OAuth 2.0 authorization code grant authorization flow.
///    See: http://tools.ietf.org/html/rfc6749#section-4.1
/// -  Performing the OAuth 2.0 implicit grant authorization flow.
///    See: http://tools.ietf.org/html/rfc6749#section-4.2
///
/// Performing OAuth 2.0 authorization:
/// 1. Set service and client/app parameters:
/// -  Client/app key & secret (as provided by the service).
/// -  The service authorization endpoint and token endpoint.
/// -  Your client/app redirect URI.
/// -  Use set_state() to assign a unique state string for the authorization
///    session (default: "").
/// -  If needed, use set_bearer_auth() to control bearer token passing in either
///    query or header (default: header). See: http://tools.ietf.org/html/rfc6750#section-2
/// -  If needed, use set_access_token_key() to set "non-standard" access token
///    key (default: "access_token").
/// -  If needed, use set_implicit_grant() to enable implicit grant flow.
/// 2. Build authorization URI with build_authorization_uri() and open this in web browser/control.
/// 3. The resource owner should then clicks "Yes" to authorize your client/app, and
///    as a result the web browser/control is redirected to redirect_uri().
/// 5. Capture the redirected URI either in web control or by HTTP listener.
/// 6. Pass the redirected URI to token_from_redirected_uri() to obtain access token.
/// -  The method ensures redirected URI contains same state() as set in step 1.
/// -  In implicit_grant() is false, this will create HTTP request to fetch access token
///    from the service. Otherwise access token is already included in the redirected URI.
///
/// Usage for issuing authenticated requests:
/// 1. Perform authorization as above to obtain the access token or use an existing token.
/// -  Some services provide option to generate access tokens for testing purposes.
/// 2. Pass the resulting oauth2_config with the access token to http_client_config::set_oauth2().
/// 3. Construct http_client with this http_client_config. As a result, all HTTP requests
///    by that client will be OAuth 2.0 authenticated.
///
/// </summary>
class oauth2_config
{
public:

    oauth2_config(utility::string_t client_key, utility::string_t client_secret,
            utility::string_t auth_endpoint, utility::string_t token_endpoint,
            utility::string_t redirect_uri, utility::string_t scope=utility::string_t(),
            utility::string_t user_agent=utility::string_t()) :
                m_client_key(std::move(client_key)),
                m_client_secret(std::move(client_secret)),
                m_auth_endpoint(std::move(auth_endpoint)),
                m_token_endpoint(std::move(token_endpoint)),
                m_redirect_uri(std::move(redirect_uri)),
                m_scope(std::move(scope)),
                m_user_agent(std::move(user_agent)),
                m_implicit_grant(false),
                m_bearer_auth(true),
                m_http_basic_auth(true),
                m_access_token_key(details::oauth2_strings::access_token)
    {}

    /// <summary>
    /// Builds an authorization URI to be loaded in the web browser/view.
    /// The URI is built with auth_endpoint() as basis.
    /// The implicit_grant() affects the built URI by selecting
    /// either authorization code or implicit grant flow.
    /// You can set generate_state to generate a new random state string.
    /// </summary>
    /// <param name="generate_state">If true, a new random state() string is generated
    /// which replaces the current state(). If false, state() is unchanged and used as-is.</param>
    /// <returns>Authorization URI string.</returns>
    _ASYNCRTIMP utility::string_t build_authorization_uri(bool generate_state);

    /// <summary>
    /// Fetch an access token (and possibly a refresh token) based on redirected URI.
    /// Behavior depends on the implicit_grant() setting.
    /// If implicit_grant() is false, the URI is parsed for 'code'
    /// parameter, and then token_from_code() is called with this code.
    /// See: http://tools.ietf.org/html/rfc6749#section-4.1
    /// Otherwise, redirect URI fragment part is parsed for 'access_token'
    /// parameter, which directly contains the token(s).
    /// See: http://tools.ietf.org/html/rfc6749#section-4.2
    /// In both cases, the 'state' parameter is parsed and is verified to match state().
    /// </summary>
    /// <param name="redirected_uri">The URI where web browser/view was redirected after resource owner's authorization.</param>
    /// <returns>Task that fetches the token(s) based on redirected URI.</returns>
    _ASYNCRTIMP pplx::task<void> token_from_redirected_uri(const web::http::uri& redirected_uri);

    /// <summary>
    /// Fetches an access token (and possibly a refresh token) from the token endpoint.
    /// The task creates an HTTP request to the token_endpoint() which exchanges
    /// the authorization code for the token(s).
    /// This also sets the refresh token if one was returned.
    /// See: http://tools.ietf.org/html/rfc6749#section-4.1.3
    /// </summary>
    /// <param name="authorization_code">Code received via redirect upon successful authorization.</param>
    /// <returns>Task that fetches token(s) based on the authorization code.</returns>
    pplx::task<void> token_from_code(utility::string_t authorization_code)
    {
        uri_builder ub;
        ub.append_query(details::oauth2_strings::grant_type, details::oauth2_strings::authorization_code, false);
        ub.append_query(details::oauth2_strings::code, uri::encode_data_string(std::move(authorization_code)), false);
        ub.append_query(details::oauth2_strings::redirect_uri, uri::encode_data_string(redirect_uri()), false);
        return _request_token(ub);
    }

    /// <summary>
    /// Fetches a new access token (and possibly a new refresh token) using the refresh token.
    /// The task creates a HTTP request to the token_endpoint().
    /// If successful, resulting access token is set as active via set_token().
    /// See: http://tools.ietf.org/html/rfc6749#section-6
    /// This also sets a new refresh token if one was returned.
    /// </summary>
    /// <returns>Task that fetches the token(s) using the refresh token.</returns>
    pplx::task<void> token_from_refresh()
    {
        uri_builder ub;
        ub.append_query(details::oauth2_strings::grant_type, details::oauth2_strings::refresh_token, false);
        ub.append_query(details::oauth2_strings::refresh_token, uri::encode_data_string(token().refresh_token()), false);
        return _request_token(ub);
    }

    /// <summary>
    /// Returns enabled state of the configuration.
    /// The oauth2_handler will perform OAuth 2.0 authentication only if
    /// this method returns true.
    /// Return value is true if access token is valid (=fetched or manually set).
    /// </summary>
    /// <returns>The configuration enabled state.</returns>
    bool is_enabled() const { return token().is_valid_access_token(); }

    /// <summary>
    /// Get client key.
    /// </summary>
    /// <returns>Client key string.</returns>
    const utility::string_t& client_key() const { return m_client_key; }
    /// <summary>
    /// Set client key.
    /// </summary>
    /// <param name="client_key">Client key string to set.</param>
    void set_client_key(utility::string_t client_key) { m_client_key = std::move(client_key); }

    /// <summary>
    /// Get client secret.
    /// </summary>
    /// <returns>Client secret string.</returns>
    const utility::string_t& client_secret() const { return m_client_secret; }
    /// <summary>
    /// Set client secret.
    /// </summary>
    /// <param name="client_secret">Client secret string to set.</param>
    void set_client_secret(utility::string_t client_secret) { m_client_secret = std::move(client_secret); }

    /// <summary>
    /// Get authorization endpoint URI string.
    /// </summary>
    /// <returns>Authorization endpoint URI string.</returns>
    const utility::string_t& auth_endpoint() const { return m_auth_endpoint; }
    /// <summary>
    /// Set authorization endpoint URI string.
    /// </summary>
    /// <param name="auth_endpoint">Authorization endpoint URI string to set.</param>
    void set_auth_endpoint(utility::string_t auth_endpoint) { m_auth_endpoint = std::move(auth_endpoint); }

    /// <summary>
    /// Get token endpoint URI string.
    /// </summary>
    /// <returns>Token endpoint URI string.</returns>
    const utility::string_t& token_endpoint() const { return m_token_endpoint; }
    /// <summary>
    /// Set token endpoint URI string.
    /// </summary>
    /// <param name="token_endpoint">Token endpoint URI string to set.</param>
    void set_token_endpoint(utility::string_t token_endpoint) { m_token_endpoint = std::move(token_endpoint); }

    /// <summary>
    /// Get redirect URI string.
    /// </summary>
    /// <returns>Redirect URI string.</returns>
    const utility::string_t& redirect_uri() const { return m_redirect_uri; }
    /// <summary>
    /// Set redirect URI string.
    /// </summary>
    /// <param name="redirect_uri">Redirect URI string to set.</param>
    void set_redirect_uri(utility::string_t redirect_uri) { m_redirect_uri = std::move(redirect_uri); }

    /// <summary>
    /// Get scope used in authorization for token.
    /// </summary>
    /// <returns>Scope string used in authorization.</returns>
    const utility::string_t& scope() const { return m_scope; }
    /// <summary>
    /// Set scope for authorization for token.
    /// </summary>
    /// <param name="scope">Scope string for authorization for token.</param>
    void set_scope(utility::string_t scope) { m_scope = std::move(scope); }

    /// <summary>
    /// Get client state string used in authorization.
    /// </summary>
    /// <returns>Client state string used in authorization.</returns>
    const utility::string_t& state() { return m_state; }
    /// <summary>
    /// Set client state string for authorization for token.
    /// The state string is used in authorization for security reasons
    /// (to uniquely identify authorization sessions).
    /// If desired, suitably secure state string can be automatically generated
    /// by build_authorization_uri().
    /// A good state string consist of 30 or more random alphanumeric characters.
    /// </summary>
    /// <param name="state">Client authorization state string to set.</param>
    void set_state(utility::string_t state) { m_state = std::move(state); }

    /// <summary>
    /// Get token.
    /// </summary>
    /// <returns>Token.</returns>
    const oauth2_token& token() const { return m_token; }
    /// <summary>
    /// Set token.
    /// </summary>
    /// <param name="token">Token to set.</param>
    void set_token(oauth2_token token) { m_token = std::move(token); }

    /// <summary>
    /// Get implicit grant setting for authorization.
    /// </summary>
    /// <returns>Implicit grant setting for authorization.</returns>
    bool implicit_grant() const { return m_implicit_grant; }
    /// <summary>
    /// Set implicit grant setting for authorization.
    /// False means authorization code grant is used for authorization.
    /// True means implicit grant is used.
    /// Default: False.
    /// </summary>
    /// <param name="implicit_grant">The implicit grant setting to set.</param>
    void set_implicit_grant(bool implicit_grant) { m_implicit_grant = implicit_grant; }

    /// <summary>
    /// Get bearer token authentication setting.
    /// </summary>
    /// <returns>Bearer token authentication setting.</returns>
    bool bearer_auth() const { return m_bearer_auth; }
    /// <summary>
    /// Set bearer token authentication setting.
    /// This must be selected based on what the service accepts.
    /// True means access token is passed in the request header. (http://tools.ietf.org/html/rfc6750#section-2.1)
    /// False means access token in passed in the query parameters. (http://tools.ietf.org/html/rfc6750#section-2.3)
    /// Default: True.
    /// </summary>
    /// <param name="bearer_auth">The bearer token authentication setting to set.</param>
    void set_bearer_auth(bool bearer_auth) { m_bearer_auth = bearer_auth; }

    /// <summary>
    /// Get HTTP Basic authentication setting for token endpoint.
    /// </summary>
    /// <returns>HTTP Basic authentication setting for token endpoint.</returns>
    bool http_basic_auth() const { return m_http_basic_auth; }
    /// <summary>
    /// Set HTTP Basic authentication setting for token endpoint.
    /// This setting must be selected based on what the service accepts.
    /// True means HTTP Basic authentication is used for the token endpoint.
    /// False means client key & secret are passed in the HTTP request body.
    /// Default: True.
    /// </summary>
    /// <param name="http_basic_auth">The HTTP Basic authentication setting to set.</param>
    void set_http_basic_auth(bool http_basic_auth) { m_http_basic_auth = http_basic_auth; }

    /// <summary>
    /// Get access token key.
    /// </summary>
    /// <returns>Access token key string.</returns>
    const utility::string_t&  access_token_key() const { return m_access_token_key; }
    /// <summary>
    /// Set access token key.
    /// If the service requires a "non-standard" key you must set it here.
    /// Default: "access_token".
    /// </summary>
    void set_access_token_key(utility::string_t access_token_key) { m_access_token_key = std::move(access_token_key); }
	
    /// <summary>
    /// Get the web proxy object
    /// </summary>
    /// <returns>A reference to the web proxy object.</returns>
    const web_proxy& proxy() const
    {
        return m_proxy;
    }

    /// <summary>
    /// Set the web proxy object that will be used by token_from_code and token_from_refresh
    /// </summary>
    /// <param name="proxy">A reference to the web proxy object.</param>
    void set_proxy(const web_proxy& proxy)
    {
        m_proxy = proxy;
    }
    
    /// <summary>
    /// Get user agent to be used in oauth2 flows.
    /// </summary>
    /// <returns>User agent string.</returns>
    const utility::string_t&  user_agent() const { return m_user_agent; }
    /// <summary>
    /// Set user agent to be used in oauth2 flows.
    /// If none is provided a default user agent is provided.
    /// </summary>
    void set_user_agent(utility::string_t user_agent) { m_user_agent = std::move(user_agent); }

private:
    friend class web::http::client::http_client_config;
    friend class web::http::oauth2::details::oauth2_handler;

    oauth2_config() :
        m_implicit_grant(false),
        m_bearer_auth(true),
        m_http_basic_auth(true)
    {}

    _ASYNCRTIMP pplx::task<void> _request_token(uri_builder& request_body);

    oauth2_token _parse_token_from_json(const json::value& token_json);

    void _authenticate_request(http_request &req) const
    {
        if (bearer_auth())
        {
            req.headers().add(header_names::authorization, _XPLATSTR("Bearer ") + token().access_token());
        }
        else
        {
            uri_builder ub(req.request_uri());
            ub.append_query(access_token_key(), token().access_token());
            req.set_request_uri(ub.to_uri());
        }
    }

    utility::string_t m_client_key;
    utility::string_t m_client_secret;
    utility::string_t m_auth_endpoint;
    utility::string_t m_token_endpoint;
    utility::string_t m_redirect_uri;
    utility::string_t m_scope;
    utility::string_t m_state;
    utility::string_t m_user_agent;

    web::web_proxy m_proxy;

    bool m_implicit_grant;
    bool m_bearer_auth;
    bool m_http_basic_auth;
    utility::string_t m_access_token_key;

    oauth2_token m_token;

    utility::nonce_generator m_state_generator;
};

} // namespace web::http::oauth2::experimental

namespace details
{

class oauth2_handler : public http_pipeline_stage
{
public:
    oauth2_handler(std::shared_ptr<experimental::oauth2_config> cfg) :
        m_config(std::move(cfg))
    {}

    virtual pplx::task<http_response> propagate(http_request request) override
    {
        if (m_config)
        {
            m_config->_authenticate_request(request);
        }
        return next_stage()->propagate(request);
    }

private:
    std::shared_ptr<experimental::oauth2_config> m_config;
};

}}}}

#endif
