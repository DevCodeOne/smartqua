#pragma once

#include <cstdlib>
#include <optional>
#include <array>

#include "utils/sd_filesystem.h"
#include "utils/filesystem_utils.h"

template<size_t CertBufferLen, size_t KeyBufferLen>
class ssl_credentials {
    public:

        ssl_credentials(const char *path_cert, const char *path_key) {
            sd_filesystem filesystem;
            // Terminating \0 is needed, therefore size + 1
            m_cert_len = load_file_completly_into_buffer(path_cert, m_cert.data(), m_cert.size() - 1);
            m_key_len = load_file_completly_into_buffer(path_key, m_key.data(), m_key.size() - 1);
            m_cert[m_cert_len] = '\0';
            m_key[m_key_len] = '\0';

            if (m_cert_len != -1 && m_key_len != -1) {
                // Accomodate terminating \0
                m_valid = true;
                m_cert_len += 1;
                m_key_len += 1;
            } else {
                m_valid = false;
            }
        }

        ssl_credentials(const ssl_credentials &other) = delete;
        ssl_credentials(ssl_credentials &&other) = delete;
        ~ssl_credentials() = default;

        ssl_credentials &operator=(const ssl_credentials &other) = delete;
        ssl_credentials &operator=(ssl_credentials &&other) = delete;

        const char *cert_begin() const {
            if (!m_valid) {
                return nullptr;
            }

            return m_cert.data();
        }

        const char *key_begin() const {
            if (!m_valid) {
                return nullptr;
            }

            return m_key.data();
        }

        size_t cert_len() const {
            return m_cert_len;
        }

        size_t key_len() const {
            return m_key_len;
        }

        explicit operator bool() const {
            return m_valid;
        }

    private:

        std::array<char, CertBufferLen> m_cert;
        std::array<char, KeyBufferLen> m_key;
        size_t m_cert_len = 0;
        size_t m_key_len = 0;
        bool m_valid = false;
};